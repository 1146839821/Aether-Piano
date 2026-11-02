#include "dsp/PerformanceEngine.h"
#include "model/NoteParameters.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
std::atomic<bool> countAllocations{false};
std::atomic<std::uint64_t> allocationCount{0};
}

void* operator new(std::size_t size) {
    if (countAllocations.load(std::memory_order_relaxed))
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (countAllocations.load(std::memory_order_relaxed))
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    int failures = 0;
    const auto check = [&](bool condition, const char* name) {
        if (!condition) {
            std::cerr << "FAIL " << name << '\n';
            ++failures;
        }
    };

    std::array<aether::model::NoteParameters, aether::model::midiNoteCount> equalBank{};
    check(aether::model::makeNoteParameterBank({}, equalBank), "build estimated 88-note bank");
    bool allEstimatedAndValid = true;
    for (std::size_t index = 0; index < equalBank.size(); ++index) {
        const auto& note = equalBank[index];
        allEstimatedAndValid = allEstimatedAndValid
            && note.midiNote == aether::model::firstMidiNote + index
            && note.calibrationStatus == aether::model::CalibrationStatus::estimated
            && !note.sourceRef.empty()
            && std::isfinite(note.targetFrequency_Hz)
            && note.string.tension_N >= 1.0 && note.string.tension_N <= 5000.0
            && note.string.length_m >= 0.10 && note.string.length_m <= 3.0;
    }
    check(allEstimatedAndValid, "all pitches have explicit estimated metadata and physical bounds");

    aether::model::NoteParameters a4{};
    aether::model::NoteParameters c4{};
    check(aether::model::makeNoteParameters(69, {}, a4), "make A4 parameters");
    check(aether::model::makeNoteParameters(60, {}, c4), "make C4 parameters");
    check(std::abs(a4.targetFrequency_Hz - 440.0) < 1.0e-10,
          "equal tuning keeps reference A4 at 440 Hz");
    check(std::abs(c4.targetFrequency_Hz - 261.6255653005986) < 1.0e-9,
          "equal tuning uses twelve-tone C4");
    aether::model::TuningConfiguration stretched;
    stretched.stretchCentsPerOctaveSquared = 0.25;
    aether::model::NoteParameters stretchedA4{}, stretchedA0{}, stretchedC8{};
    check(aether::model::makeNoteParameters(69, stretched, stretchedA4)
          && aether::model::makeNoteParameters(21, stretched, stretchedA0)
          && aether::model::makeNoteParameters(108, stretched, stretchedC8),
          "make stretched tuning endpoints");
    check(std::abs(stretchedA4.targetFrequency_Hz - 440.0) < 1.0e-10
          && stretchedA0.targetFrequency_Hz < equalBank.front().targetFrequency_Hz
          && stretchedC8.targetFrequency_Hz > equalBank.back().targetFrequency_Hz,
          "stretch curve pivots around A4");
    aether::model::NoteParameters invalidNote{};
    aether::model::TuningConfiguration noInharmonicity;
    noInharmonicity.inharmonicityScale = 0.0;
    aether::model::NoteParameters c4NoInharmonicity{};
    check(aether::model::makeNoteParameters(60, noInharmonicity, c4NoInharmonicity)
          && c4NoInharmonicity.string.inharmonicityB == 0.0
          && std::abs(c4NoInharmonicity.targetFrequency_Hz - c4.targetFrequency_Hz) < 1.0e-12,
          "zero inharmonicity changes B without shifting target f0");
    aether::model::TuningConfiguration invalidInharmonicity;
    invalidInharmonicity.inharmonicityScale = 4.1;
    check(!aether::model::makeNoteParameters(60, invalidInharmonicity, invalidNote),
          "reject out-of-range inharmonicity scale");
    check(!aether::model::makeNoteParameters(20, {}, invalidNote), "reject MIDI note below A0");
    aether::model::TuningConfiguration invalidTuning;
    invalidTuning.referenceA4_Hz = 0.0;
    check(!aether::model::makeNoteParameters(60, invalidTuning, invalidNote),
          "reject invalid reference tuning");

    aether::PerformanceEngine engine;
    check(engine.prepare(44100.0, 512), "prepare full performance engine at 44.1 kHz");
    check(engine.isPrepared(), "engine reports prepared");
    check(engine.diagnostics().activeVoiceCount == 0
          && engine.diagnostics().peakActiveVoiceCount == 0
          && engine.diagnostics().nonFiniteOutputs == 0,
          "prepare-time DSP warm-up leaves no active voices or diagnostic state");
    check(engine.noteParameters(21) != nullptr && engine.noteParameters(108) != nullptr
          && engine.noteParameters(20) == nullptr, "parameter lookup matches MIDI 21..108");

    std::array<float, 512> left{}, right{};
    bool allPitchesFiniteAndAudible = true;
    for (std::uint8_t midiNote = aether::model::firstMidiNote;
         midiNote <= aether::model::lastMidiNote; ++midiNote) {
        engine.reset();
        engine.handleEvent({0, 0x90, midiNote, 80});
        double peak = 0.0;
        for (int block = 0; block < 32; ++block) {
            engine.process(left.data(), right.data(), 512);
            for (const float sample : left) {
                allPitchesFiniteAndAudible = allPitchesFiniteAndAudible && std::isfinite(sample);
                peak = std::max(peak, std::abs(static_cast<double>(sample)));
            }
        }
        allPitchesFiniteAndAudible = allPitchesFiniteAndAudible && peak > 1.0e-9;
    }
    check(allPitchesFiniteAndAudible, "all 88 MIDI pitches produce finite nonzero output");
    bool sampleRateExtremesPrepare = true;
    for (const double rate : {44100.0, 48000.0, 96000.0, 192000.0}) {
        aether::PerformanceEngine rateEngine;
        sampleRateExtremesPrepare = sampleRateExtremesPrepare
            && rateEngine.prepare(rate, 64);
        for (const std::uint8_t midiNote : {aether::model::firstMidiNote,
                                            aether::model::lastMidiNote}) {
            rateEngine.reset();
            rateEngine.handleEvent({0, 0x90, midiNote, 80});
            rateEngine.process(left.data(), right.data(), 64);
            sampleRateExtremesPrepare = sampleRateExtremesPrepare
                && std::all_of(left.begin(), left.begin() + 64,
                               [](float sample) { return std::isfinite(sample); });
        }
    }
    check(sampleRateExtremesPrepare,
          "A0 and C8 prepare and render finitely at 44.1/48/96/192 kHz");
    aether::PerformanceEngine unsupportedLowRateEngine;
    check(!unsupportedLowRateEngine.prepare(22050.0, 512),
          "reject rates too low for the C8 delay geometry");

    aether::PerformanceEngine offsetEngine, segmentedEngine;
    check(offsetEngine.prepare(48000.0, 512) && segmentedEngine.prepare(48000.0, 512),
          "prepare sample-offset engines");
    std::array<float, 512> offsetLeft{}, offsetRight{}, segmentedLeft{}, segmentedRight{};
    const std::array<aether::MidiEvent, 2> scheduled{{
        {97, 0x90, 60, 80}, {301, 0x80, 60, 0}}};
    offsetEngine.process(offsetLeft.data(), offsetRight.data(), 512, scheduled);
    segmentedEngine.process(segmentedLeft.data(), segmentedRight.data(), 97);
    segmentedEngine.handleEvent({0, 0x90, 60, 80});
    segmentedEngine.process(segmentedLeft.data() + 97, segmentedRight.data() + 97, 204);
    segmentedEngine.handleEvent({0, 0x80, 60, 0});
    segmentedEngine.process(segmentedLeft.data() + 301, segmentedRight.data() + 301, 211);
    check(std::equal(offsetLeft.begin(), offsetLeft.end(), segmentedLeft.begin()),
          "sample-accurate events match segmented callback boundaries");

    aether::PerformanceEngine variableBlockEngine, variableSegmentedEngine;
    check(variableBlockEngine.prepare(48000.0, 64)
          && variableSegmentedEngine.prepare(48000.0, 64),
          "prepare engines with a small advertised block size");
    std::array<float, 1024> variableLeft{}, variableRight{}, variableSegmentedLeft{},
        variableSegmentedRight{};
    const std::array<aether::MidiEvent, 2> variableEvents{{
        {511, 0x90, 60, 80}, {512, 0x80, 60, 0}}};
    variableBlockEngine.process(variableLeft.data(), variableRight.data(), 1024,
                                variableEvents);
    variableSegmentedEngine.process(variableSegmentedLeft.data(),
                                    variableSegmentedRight.data(), 511);
    variableSegmentedEngine.handleEvent({0, 0x90, 60, 80});
    variableSegmentedEngine.process(variableSegmentedLeft.data() + 511,
                                    variableSegmentedRight.data() + 511, 1);
    variableSegmentedEngine.handleEvent({0, 0x80, 60, 0});
    variableSegmentedEngine.process(variableSegmentedLeft.data() + 512,
                                    variableSegmentedRight.data() + 512, 512);
    check(std::equal(variableLeft.begin(), variableLeft.end(),
                     variableSegmentedLeft.begin()),
          "large variable block and exact block-edge MIDI events match segmented processing");
    variableBlockEngine.process(left.data(), right.data(), 0);

    aether::PerformanceEngine chordEngine;
    check(chordEngine.prepare(48000.0, 512), "prepare chord engine");
    const std::array<aether::MidiEvent, 4> chord{{
        {0, 0x90, 48, 80}, {0, 0x90, 52, 80},
        {0, 0x90, 55, 80}, {0, 0x90, 60, 80}}};
    chordEngine.process(left.data(), right.data(), 512, chord);
    check(chordEngine.diagnostics().activeVoiceCount == 4
          && chordEngine.diagnostics().peakActiveVoiceCount == 4,
          "four simultaneous notes allocate four persistent voices");
    const std::array<aether::MidiEvent, 1> outsideRange{{{0, 0x90, 109, 80}}};
    chordEngine.process(left.data(), right.data(), 512, outsideRange);
    check(chordEngine.diagnostics().rejectedEvents == 1,
          "reject notes outside MIDI 21..108");

    aether::PerformanceEngine repeatedEngine, freshEngine;
    check(repeatedEngine.prepare(48000.0, 4096) && freshEngine.prepare(48000.0, 4096),
          "prepare retrigger engines");
    repeatedEngine.handleEvent({0, 0x90, 60, 80});
    repeatedEngine.process(left.data(), right.data(), 512);
    repeatedEngine.handleEvent({0, 0x90, 60, 80});
    freshEngine.handleEvent({0, 0x90, 60, 80});
    std::array<float, 512> retriggerLeft{}, retriggerRight{}, freshLeft{}, freshRight{};
    repeatedEngine.process(retriggerLeft.data(), retriggerRight.data(), 512);
    freshEngine.process(freshLeft.data(), freshRight.data(), 512);
    double retainedWaveDifference = 0.0;
    for (std::size_t index = 0; index < retriggerLeft.size(); ++index)
        retainedWaveDifference = std::max(retainedWaveDifference,
            std::abs(static_cast<double>(retriggerLeft[index]) - freshLeft[index]));
    check(retainedWaveDifference > 1.0e-8
          && repeatedEngine.diagnostics().activeVoiceCount == 1,
          "same-pitch retrigger reuses one voice and preserves string waves");
    repeatedEngine.handleEvent({0, 0x80, 60, 0});
    check(repeatedEngine.diagnostics().activeVoiceCount == 1,
          "first overlapping Note Off keeps the shared key held");
    repeatedEngine.handleEvent({0, 0x80, 60, 0});
    check(repeatedEngine.diagnostics().acceptedNoteOns == 2
          && repeatedEngine.diagnostics().acceptedNoteOffs == 2,
          "overlapping repeated notes pair by pitch count");
    repeatedEngine.handleEvent({0, 0x80, 60, 0});
    check(repeatedEngine.diagnostics().unmatchedNoteOffs == 1,
          "unmatched Note Off is counted and ignored");

    aether::PerformanceEngine pedalEngine, dryReleaseEngine;
    check(pedalEngine.prepare(48000.0, 512) && dryReleaseEngine.prepare(48000.0, 512),
          "prepare sustain comparison engines");
    const std::array<aether::MidiEvent, 2> pedalOn{{
        {0, 0xb0, 64, 127}, {0, 0x90, 60, 80}}};
    const std::array<aether::MidiEvent, 1> noteOn{{{0, 0x90, 60, 80}}};
    pedalEngine.process(left.data(), right.data(), 512, pedalOn);
    dryReleaseEngine.process(left.data(), right.data(), 512, noteOn);
    for (int block = 1; block < 48; ++block) {
        if (block == 12) {
            pedalEngine.handleEvent({0, 0x80, 60, 0});
            dryReleaseEngine.handleEvent({0, 0x80, 60, 0});
        }
        pedalEngine.process(left.data(), right.data(), 512);
        dryReleaseEngine.process(retriggerLeft.data(), retriggerRight.data(), 512);
    }
    double pedalEnergy = 0.0;
    double dryReleaseEnergy = 0.0;
    for (int block = 0; block < 24; ++block) {
        pedalEngine.process(left.data(), right.data(), 512);
        dryReleaseEngine.process(retriggerLeft.data(), retriggerRight.data(), 512);
        for (std::size_t sample = 0; sample < left.size(); ++sample) {
            pedalEnergy += static_cast<double>(left[sample]) * left[sample];
            dryReleaseEnergy += static_cast<double>(retriggerLeft[sample]) * retriggerLeft[sample];
        }
    }
    check(pedalEnergy > dryReleaseEnergy * 10.0,
          "CC64 holds released string energy until pedal lift");
    pedalEngine.handleEvent({0, 0xb0, 64, 0});
    check(pedalEngine.diagnostics().activeVoiceCount == 1,
          "pedal lift begins release without dropping the voice immediately");

    aether::PerformanceEngine releasedThenPedalEngine, plainReleaseEngine;
    check(releasedThenPedalEngine.prepare(48000.0, 512)
          && plainReleaseEngine.prepare(48000.0, 512),
          "prepare released-note pedal-order engines");
    releasedThenPedalEngine.handleEvent({0, 0x90, 60, 80});
    plainReleaseEngine.handleEvent({0, 0x90, 60, 80});
    releasedThenPedalEngine.process(left.data(), right.data(), 512);
    plainReleaseEngine.process(left.data(), right.data(), 512);
    releasedThenPedalEngine.handleEvent({0, 0x80, 60, 0});
    plainReleaseEngine.handleEvent({0, 0x80, 60, 0});
    releasedThenPedalEngine.process(left.data(), right.data(), 64);
    plainReleaseEngine.process(left.data(), right.data(), 64);
    releasedThenPedalEngine.handleEvent({0, 0xb0, 64, 127});
    std::array<float, 256> releasedThenPedalLeft{}, releasedThenPedalRight{};
    std::array<float, 256> plainReleaseLeft{}, plainReleaseRight{};
    releasedThenPedalEngine.process(releasedThenPedalLeft.data(),
                                    releasedThenPedalRight.data(), 256);
    plainReleaseEngine.process(plainReleaseLeft.data(), plainReleaseRight.data(), 256);
    bool pedalDownPreservesPriorRelease = true;
    for (std::size_t sample = 0; sample < plainReleaseLeft.size(); ++sample)
        pedalDownPreservesPriorRelease = pedalDownPreservesPriorRelease
            && releasedThenPedalLeft[sample] == plainReleaseLeft[sample];
    check(pedalDownPreservesPriorRelease,
          "CC64 down does not revive a note whose release already began");

    aether::PerformanceEngine releaseEngine;
    check(releaseEngine.prepare(48000.0, 512), "prepare release lifecycle engine");
    releaseEngine.handleEvent({0, 0x90, 60, 80});
    releaseEngine.process(left.data(), right.data(), 512);
    releaseEngine.handleEvent({0, 0x80, 60, 0});
    for (int block = 0; block < 220; ++block)
        releaseEngine.process(left.data(), right.data(), 512);
    check(releaseEngine.diagnostics().activeVoiceCount == 0,
          "released voice resets after quiet threshold and hysteresis dwell");

    aether::PerformanceEngine allocationEngine;
    check(allocationEngine.prepare(48000.0, 512), "prepare allocation probe engine");
    for (std::uint8_t midiNote = 21; midiNote < 85; ++midiNote)
        allocationEngine.handleEvent({0, 0x90, midiNote, 80});
    check(allocationEngine.diagnostics().activeVoiceCount == 64
          && allocationEngine.diagnostics().peakActiveVoiceCount == 64,
          "allocation probe starts 64 simultaneous voices");
    const auto allocationsBefore = allocationCount.load(std::memory_order_relaxed);
    countAllocations.store(true, std::memory_order_relaxed);
    for (int block = 0; block < 64; ++block) {
        if (block == 16)
            allocationEngine.handleEvent({0, 0xb0, 64, 127});
        if (block == 20) {
            for (std::uint8_t midiNote = 21; midiNote < 85; ++midiNote)
                allocationEngine.handleEvent({0, 0x80, midiNote, 0});
        }
        if (block == 30)
            allocationEngine.handleEvent({0, 0xb0, 64, 0});
        allocationEngine.process(left.data(), right.data(), 512);
    }
    countAllocations.store(false, std::memory_order_relaxed);
    check(allocationEngine.diagnostics().acceptedNoteOffs == 64,
          "64-voice probe releases all notes under CC64");
    check(allocationCount.load(std::memory_order_relaxed) == allocationsBefore,
          "64-voice sustain processing issues no global new or new[] calls");

    return failures == 0 ? 0 : 1;
}
