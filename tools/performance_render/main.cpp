#include "dsp/PerformanceEngine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <string>

namespace {
struct ScheduledEvent {
    std::uint64_t sample = 0;
    aether::MidiEvent event{};
};

struct Scenario {
    std::array<ScheduledEvent, 256> events{};
    std::size_t eventCount = 0;
    double duration_s = 0.0;
};

bool addEvent(Scenario& scenario, std::uint32_t sampleRate, double time_s,
              std::uint8_t status, std::uint8_t note, std::uint8_t velocity) {
    if (scenario.eventCount >= scenario.events.size() || !std::isfinite(time_s) || time_s < 0.0)
        return false;
    scenario.events[scenario.eventCount++] = {
        static_cast<std::uint64_t>(time_s * sampleRate),
        {0, status, note, velocity}};
    return true;
}

bool addOnOff(Scenario& scenario, std::uint32_t sampleRate, double on_s,
              double off_s, std::uint8_t note, std::uint8_t velocity) {
    return addEvent(scenario, sampleRate, on_s, 0x90, note, velocity)
        && addEvent(scenario, sampleRate, off_s, 0x80, note, 0);
}

bool buildScenario(const std::string& name, std::uint32_t sampleRate,
                   std::uint8_t singleNote, std::uint8_t velocity,
                   double durationOverride_s, double noteOff_s, Scenario& scenario) {
    constexpr double tail_s = 3.0;
    if (name == "note") {
        scenario.duration_s = durationOverride_s > 0.0 ? durationOverride_s : 2.0;
        return addOnOff(scenario, sampleRate, 0.0, noteOff_s >= 0.0 ? noteOff_s : 1.5,
                        singleNote, velocity);
    }
    if (name == "scale") {
        constexpr double step_s = 0.13;
        for (int note = aether::model::firstMidiNote; note <= aether::model::lastMidiNote; ++note) {
            const double on_s = (note - aether::model::firstMidiNote) * step_s;
            if (!addOnOff(scenario, sampleRate, on_s, on_s + 0.10,
                          static_cast<std::uint8_t>(note), velocity))
                return false;
        }
        scenario.duration_s = (aether::model::midiNoteCount - 1) * step_s + 0.10 + tail_s;
        return true;
    }
    if (name == "arpeggio") {
        constexpr std::array<std::uint8_t, 16> notes{{
            21, 33, 45, 57, 69, 81, 93, 105, 108, 96, 84, 72, 60, 48, 36, 24}};
        constexpr double step_s = 0.16;
        for (std::size_t i = 0; i < notes.size(); ++i) {
            const double on_s = i * step_s;
            if (!addOnOff(scenario, sampleRate, on_s, on_s + 0.12, notes[i], velocity))
                return false;
        }
        scenario.duration_s = (notes.size() - 1) * step_s + 0.12 + tail_s;
        return true;
    }
    if (name == "chord") {
        constexpr std::array<std::uint8_t, 4> notes{{48, 52, 55, 60}};
        for (const auto note : notes) {
            if (!addOnOff(scenario, sampleRate, 0.0, 0.75, note, velocity))
                return false;
        }
        scenario.duration_s = 0.75 + tail_s;
        return true;
    }
    if (name == "retrigger") {
        constexpr int strikes = 6;
        constexpr double step_s = 0.030;
        for (int strike = 0; strike < strikes; ++strike) {
            const double on_s = strike * step_s;
            if (!addOnOff(scenario, sampleRate, on_s, on_s + 0.10,
                          singleNote, static_cast<std::uint8_t>(std::min(127, int(velocity) + 4 * strike))))
                return false;
        }
        scenario.duration_s = (strikes - 1) * step_s + 0.10 + tail_s;
        return true;
    }
    return false;
}

void writeU16(std::ostream& out, std::uint16_t value) {
    out.put(static_cast<char>(value));
    out.put(static_cast<char>(value >> 8));
}

void writeU32(std::ostream& out, std::uint32_t value) {
    for (int byte = 0; byte < 4; ++byte)
        out.put(static_cast<char>(value >> (8 * byte)));
}

void writeWaveHeader(std::ostream& out, std::uint32_t sampleRate, std::uint32_t frames) {
    constexpr std::uint32_t channels = 2;
    constexpr std::uint32_t bytesPerSample = 4;
    const std::uint32_t dataBytes = frames * channels * bytesPerSample;
    out.write("RIFF", 4); writeU32(out, 36 + dataBytes);
    out.write("WAVEfmt ", 8); writeU32(out, 16); writeU16(out, 3);
    writeU16(out, channels); writeU32(out, sampleRate);
    writeU32(out, sampleRate * channels * bytesPerSample);
    writeU16(out, channels * bytesPerSample); writeU16(out, 8 * bytesPerSample);
    out.write("data", 4); writeU32(out, dataBytes);
}
}

int main(int argc, char** argv) {
    std::string scenarioName;
    std::string outputPath = "performance.wav";
    std::string eventsPath;
    std::uint32_t sampleRate = 48000;
    std::uint8_t note = 60;
    std::uint8_t velocity = 80;
    double durationOverride_s = 0.0;
    double noteOff_s = -1.0;
    double stretchCentsPerOctaveSquared = 0.0;
    double inharmonicityScale = 1.0;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--scenario" && index + 1 < argc) scenarioName = argv[++index];
        else if (argument == "--output" && index + 1 < argc) outputPath = argv[++index];
        else if (argument == "--events" && index + 1 < argc) eventsPath = argv[++index];
        else if (argument == "--sample-rate" && index + 1 < argc)
            sampleRate = static_cast<std::uint32_t>(std::stoul(argv[++index]));
        else if (argument == "--note" && index + 1 < argc)
            note = static_cast<std::uint8_t>(std::stoi(argv[++index]));
        else if (argument == "--velocity" && index + 1 < argc)
            velocity = static_cast<std::uint8_t>(std::stoi(argv[++index]));
        else if (argument == "--duration" && index + 1 < argc)
            durationOverride_s = std::stod(argv[++index]);
        else if (argument == "--note-off" && index + 1 < argc)
            noteOff_s = std::stod(argv[++index]);
        else if (argument == "--stretch" && index + 1 < argc)
            stretchCentsPerOctaveSquared = std::stod(argv[++index]);
        else if (argument == "--inharmonicity-scale" && index + 1 < argc)
            inharmonicityScale = std::stod(argv[++index]);
        else {
            std::cerr << "Usage: performance_render --scenario note|scale|arpeggio|chord|retrigger "
                         "[--note 21..108] [--velocity 1..127] "
                         "[--sample-rate 44100|48000|96000|192000] "
                         "[--duration seconds] [--note-off seconds] [--stretch cents/octave^2] "
                         "[--inharmonicity-scale 0..4] "
                         "[--output file.wav] [--events file.csv]\n";
            return 2;
        }
    }
    if ((sampleRate != 44100 && sampleRate != 48000 && sampleRate != 96000
         && sampleRate != 192000)
        || note < aether::model::firstMidiNote || note > aether::model::lastMidiNote
        || velocity == 0 || velocity > 127 || !std::isfinite(durationOverride_s)
        || !std::isfinite(noteOff_s) || !std::isfinite(stretchCentsPerOctaveSquared)
        || !std::isfinite(inharmonicityScale) || inharmonicityScale < 0.0
        || inharmonicityScale > 4.0) {
        std::cerr << "invalid performance render parameter\n";
        return 2;
    }

    Scenario scenario;
    if (!buildScenario(scenarioName, sampleRate, note, velocity,
                       durationOverride_s, noteOff_s, scenario)) {
        std::cerr << "unknown scenario or event capacity exceeded\n";
        return 2;
    }
    std::stable_sort(scenario.events.begin(), scenario.events.begin() + scenario.eventCount,
        [](const ScheduledEvent& left, const ScheduledEvent& right) {
            return left.sample < right.sample;
        });
    if (!std::isfinite(scenario.duration_s) || scenario.duration_s <= 0.0
        || scenario.duration_s > 120.0) {
        std::cerr << "invalid scenario duration\n";
        return 2;
    }
    if (scenarioName == "note" && noteOff_s >= 0.0
        && noteOff_s >= scenario.duration_s) {
        std::cerr << "note-off must occur before the render ends\n";
        return 2;
    }
    const auto frameCount64 = static_cast<std::uint64_t>(scenario.duration_s * sampleRate);
    const std::uint64_t dataBytes64 = frameCount64 * 2U * sizeof(float);
    if (frameCount64 == 0 || dataBytes64 > std::numeric_limits<std::uint32_t>::max() - 36U) {
        std::cerr << "scenario exceeds RIFF/WAV size limits\n";
        return 2;
    }
    const auto frames = static_cast<std::uint32_t>(frameCount64);
    for (std::size_t index = 0; index < scenario.eventCount; ++index) {
        if (scenario.events[index].sample >= frames) {
            std::cerr << "scheduled MIDI event falls outside the rendered WAV\n";
            return 2;
        }
    }

    aether::model::TuningConfiguration tuning;
    tuning.stretchCentsPerOctaveSquared = stretchCentsPerOctaveSquared;
    tuning.inharmonicityScale = inharmonicityScale;
    aether::PerformanceEngine engine;
    if (!engine.prepare(sampleRate, 512, tuning)) {
        std::cerr << "failed to prepare 88-note performance engine\n";
        return 1;
    }
    std::ofstream wave(outputPath, std::ios::binary);
    if (!wave) {
        std::cerr << "cannot open output WAV: " << outputPath << '\n';
        return 1;
    }
    writeWaveHeader(wave, sampleRate, frames);

    std::ofstream eventFile;
    if (!eventsPath.empty()) {
        eventFile.open(eventsPath);
        if (!eventFile) {
            std::cerr << "cannot open event schedule: " << eventsPath << '\n';
            return 1;
        }
        eventFile << "sample_offset,time_s,status_hex,note,velocity\n";
        for (std::size_t index = 0; index < scenario.eventCount; ++index) {
            const auto& scheduled = scenario.events[index];
            eventFile << scheduled.sample << ','
                << static_cast<double>(scheduled.sample) / sampleRate << ",0x"
                << std::hex << static_cast<int>(scheduled.event.status) << std::dec << ','
                << static_cast<int>(scheduled.event.data1) << ','
                << static_cast<int>(scheduled.event.data2) << '\n';
        }
    }

    constexpr std::uint32_t maxBlockSize = 512;
    std::array<float, maxBlockSize> left{}, right{};
    std::array<aether::MidiEvent, 16> blockEvents{};
    std::size_t eventIndex = 0;
    double peakOutput_m_s = 0.0;
    for (std::uint32_t done = 0; done < frames;) {
        const auto count = std::min(maxBlockSize, frames - done);
        std::size_t blockEventCount = 0;
        while (eventIndex < scenario.eventCount
               && scenario.events[eventIndex].sample < static_cast<std::uint64_t>(done) + count) {
            const auto& scheduled = scenario.events[eventIndex++];
            if (scheduled.sample < done || blockEventCount >= blockEvents.size()) {
                std::cerr << "internal event schedule error\n";
                return 1;
            }
            auto event = scheduled.event;
            event.sampleOffset = static_cast<std::uint32_t>(scheduled.sample - done);
            blockEvents[blockEventCount++] = event;
        }
        engine.process(left.data(), right.data(), count,
            std::span<const aether::MidiEvent>(blockEvents.data(), blockEventCount));
        for (std::uint32_t sample = 0; sample < count; ++sample) {
            peakOutput_m_s = std::max(peakOutput_m_s, std::abs(static_cast<double>(left[sample])));
            writeU32(wave, std::bit_cast<std::uint32_t>(left[sample]));
            writeU32(wave, std::bit_cast<std::uint32_t>(right[sample]));
        }
        done += count;
    }
    if (!wave) {
        std::cerr << "failed writing WAV\n";
        return 1;
    }

    const auto& diagnostics = engine.diagnostics();
    std::cout << std::setprecision(17);
    std::cout << "scenario=" << scenarioName << " sample_rate_hz=" << sampleRate
              << " duration_s=" << scenario.duration_s << " events=" << scenario.eventCount
              << " peak_active_voices=" << diagnostics.peakActiveVoiceCount
              << " active_voices_at_end=" << diagnostics.activeVoiceCount
              << " accepted_note_ons=" << diagnostics.acceptedNoteOns
              << " accepted_note_offs=" << diagnostics.acceptedNoteOffs
              << " unmatched_note_offs=" << diagnostics.unmatchedNoteOffs
              << " non_finite_outputs=" << diagnostics.nonFiniteOutputs
              << " peak_output_velocity_m_s=" << peakOutput_m_s
              << " wave_written=1"
              << " events_csv_written=" << (!eventsPath.empty() ? 1 : 0);
    if (scenarioName == "note") {
        const auto* parameters = engine.noteParameters(note);
        std::cout << " target_f0_hz=" << parameters->targetFrequency_Hz
                  << " inharmonicity_B=" << parameters->string.inharmonicityB
                  << " fundamental_t60_s=" << parameters->string.fundamentalT60Seconds
                  << " damper_t60_s=" << parameters->string.damperT60Seconds
                  << " hammer_mass_kg=" << parameters->string.hammerMass_kg
                  << " hammer_stiffness_N_m_p=" << parameters->string.hammerStiffness_N_m_p
                  << " strike_position_fraction=" << parameters->string.strikePositionFraction
                  << " readout_position_fraction=" << parameters->string.readoutPositionFraction
                  << " calibration_status=ESTIMATED"
                  << " source_ref=" << parameters->sourceRef;
    }
    std::cout << '\n';
}
