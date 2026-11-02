#include "dsp/PerformanceEngine.h"

#include <algorithm>
#include <cmath>

namespace aether {
namespace {
constexpr double lowSilenceThreshold_m_s = 1.0e-7;
constexpr double highSilenceThreshold_m_s = 5.0e-7;
constexpr double minimumQuietDuration_s = 0.25;
}

bool PerformanceEngine::prepare(double sampleRateHz, std::uint32_t maximumBlockSize,
                                const model::TuningConfiguration& tuning) noexcept {
    prepared_ = false;
    sampleRateHz_ = 0.0;
    if (!std::isfinite(sampleRateHz) || sampleRateHz < 44100.0
        || sampleRateHz > 192000.0 || maximumBlockSize == 0)
        return false;

    if (!model::makeNoteParameterBank(tuning, notes_))
        return false;
    try {
        for (std::size_t index = 0; index < voices_.size(); ++index) {
            if (!voices_[index].string.prepare(sampleRateHz, notes_[index].string))
                return false;
        }
    } catch (...) {
        for (auto& voice : voices_)
            voice.string.reset();
        return false;
    }

    sampleRateHz_ = sampleRateHz;
    minimumQuietSamples_ = static_cast<std::uint32_t>(
        std::max(1.0, std::round(minimumQuietDuration_s * sampleRateHz)));
    prepared_ = true;
    reset();

    // Exercise every prepared string and its hot audio path before a host can
    // enter the real-time callback. Reset again so warm-up state is inaudible.
    for (auto& voice : voices_)
        (void) voice.string.noteOn(0.8);
    for (std::size_t sample = 0; sample < 128; ++sample) {
        for (auto& voice : voices_)
            (void) voice.string.processSample();
    }
    reset();
    return true;
}

void PerformanceEngine::reset() noexcept {
    for (auto& voice : voices_) {
        voice.string.reset();
        voice.quietSamples = 0;
        voice.heldNoteCount = 0;
        voice.active = false;
        voice.sustained = false;
    }
    sustainPedalDown_ = false;
    diagnostics_ = {};
}

void PerformanceEngine::process(float* left, float* right, std::uint32_t samples,
                                std::span<const MidiEvent> events) noexcept {
    if (left == nullptr || right == nullptr)
        return;
    if (!prepared_ || !validEvents(events, samples)) {
        for (std::uint32_t sample = 0; sample < samples; ++sample)
            left[sample] = right[sample] = 0.0f;
        return;
    }

    std::size_t eventIndex = 0;
    for (std::uint32_t sample = 0; sample < samples; ++sample) {
        while (eventIndex < events.size() && events[eventIndex].sampleOffset == sample)
            handleEvent(events[eventIndex++]);

        double mixedVelocity_m_s = 0.0;
        for (auto& voice : voices_) {
            if (!voice.active)
                continue;
            double outputVelocity_m_s = voice.string.processSample();
            if (!std::isfinite(outputVelocity_m_s)) {
                ++diagnostics_.nonFiniteOutputs;
                outputVelocity_m_s = 0.0;
            }
            mixedVelocity_m_s += outputVelocity_m_s;
            updateSilence(voice, outputVelocity_m_s);
        }

        if (!std::isfinite(mixedVelocity_m_s)) {
            ++diagnostics_.nonFiniteOutputs;
            mixedVelocity_m_s = 0.0;
        }
        diagnostics_.peakOutputVelocity_m_s = std::max(
            diagnostics_.peakOutputVelocity_m_s, std::abs(mixedVelocity_m_s));
        const float output = static_cast<float>(mixedVelocity_m_s);
        left[sample] = output;
        right[sample] = output;
    }
}

void PerformanceEngine::handleEvent(const MidiEvent& event) noexcept {
    if (!prepared_) {
        ++diagnostics_.rejectedEvents;
        return;
    }
    const auto status = event.status & 0xf0;
    if (status == 0x90 && event.data2 != 0) {
        if (!noteOn(event.data1, event.data2))
            ++diagnostics_.rejectedEvents;
        return;
    }
    if (status == 0x80 || (status == 0x90 && event.data2 == 0)) {
        if (event.data1 < model::firstMidiNote || event.data1 > model::lastMidiNote) {
            ++diagnostics_.rejectedEvents;
            return;
        }
        noteOff(event.data1);
        return;
    }
    if (status == 0xb0 && event.data1 == 64) {
        const bool pedalDown = event.data2 >= 64;
        if (pedalDown == sustainPedalDown_)
            return;
        sustainPedalDown_ = pedalDown;
        if (!pedalDown) {
            for (auto& voice : voices_) {
                if (!voice.active || voice.heldNoteCount != 0 || !voice.sustained)
                    continue;
                voice.sustained = false;
                setDamper(voice, true);
            }
        }
        return;
    }
    if (status == 0xb0 && (event.data1 == 120 || event.data1 == 123)) {
        for (std::size_t index = 0; index < voices_.size(); ++index) {
            auto& voice = voices_[index];
            if (!voice.active)
                continue;
            voice.heldNoteCount = 0;
            voice.sustained = sustainPedalDown_;
            if (!voice.sustained)
                setDamper(voice, true);
        }
        return;
    }
}

bool PerformanceEngine::validEvents(std::span<const MidiEvent> events,
                                    std::uint32_t samples) const noexcept {
    std::uint32_t previousOffset = 0;
    bool first = true;
    for (const auto& event : events) {
        if (event.sampleOffset >= samples
            || (!first && event.sampleOffset < previousOffset))
            return false;
        previousOffset = event.sampleOffset;
        first = false;
    }
    return true;
}

const model::NoteParameters* PerformanceEngine::noteParameters(std::uint8_t midiNote) const noexcept {
    if (!prepared_ || midiNote < model::firstMidiNote || midiNote > model::lastMidiNote)
        return nullptr;
    return &notes_[static_cast<std::size_t>(midiNote - model::firstMidiNote)];
}

double PerformanceEngine::hammerSpeedForVelocity(std::uint8_t velocity) const noexcept {
    struct Anchor { double velocity; double speed_m_s; };
    constexpr std::array<Anchor, 5> anchors{{
        {1.0, 0.1}, {24.0, 0.3}, {64.0, 0.8}, {112.0, 1.6}, {127.0, 1.9}
    }};
    const double value = std::clamp<double>(velocity, 1.0, 127.0);
    for (std::size_t index = 1; index < anchors.size(); ++index) {
        if (value <= anchors[index].velocity) {
            const auto& lower = anchors[index - 1];
            const auto& upper = anchors[index];
            const double fraction = (value - lower.velocity)
                                  / (upper.velocity - lower.velocity);
            return lower.speed_m_s + fraction * (upper.speed_m_s - lower.speed_m_s);
        }
    }
    return anchors.back().speed_m_s;
}

bool PerformanceEngine::noteOn(std::uint8_t midiNote, std::uint8_t velocity) noexcept {
    if (midiNote < model::firstMidiNote || midiNote > model::lastMidiNote)
        return false;
    auto& voice = voices_[static_cast<std::size_t>(midiNote - model::firstMidiNote)];
    if (!voice.active) {
        voice.active = true;
        ++diagnostics_.activeVoiceCount;
        diagnostics_.peakActiveVoiceCount = std::max(
            diagnostics_.peakActiveVoiceCount, diagnostics_.activeVoiceCount);
    }
    if (voice.heldNoteCount < 255)
        ++voice.heldNoteCount;
    voice.sustained = false;
    voice.quietSamples = 0;
    voice.string.setDamperEngaged(false);
    if (!voice.string.noteOn(hammerSpeedForVelocity(velocity))) {
        if (voice.heldNoteCount > 0)
            --voice.heldNoteCount;
        if (voice.heldNoteCount == 0)
            setDamper(voice, true);
        return false;
    }
    ++diagnostics_.acceptedNoteOns;
    return true;
}

void PerformanceEngine::noteOff(std::uint8_t midiNote) noexcept {
    auto& voice = voices_[static_cast<std::size_t>(midiNote - model::firstMidiNote)];
    if (voice.heldNoteCount == 0) {
        ++diagnostics_.unmatchedNoteOffs;
        return;
    }
    --voice.heldNoteCount;
    ++diagnostics_.acceptedNoteOffs;
    if (voice.heldNoteCount != 0)
        return;
    if (sustainPedalDown_) {
        voice.sustained = true;
        voice.quietSamples = 0;
    } else {
        setDamper(voice, true);
    }
}

void PerformanceEngine::setDamper(Voice& voice, bool engaged) noexcept {
    voice.sustained = false;
    voice.string.setDamperEngaged(engaged);
    voice.quietSamples = 0;
}

void PerformanceEngine::updateSilence(Voice& voice, double outputVelocity_m_s) noexcept {
    if (voice.heldNoteCount != 0 || voice.sustained)
        return;
    const double magnitude = std::abs(outputVelocity_m_s);
    if (magnitude < lowSilenceThreshold_m_s) {
        if (voice.quietSamples < minimumQuietSamples_)
            ++voice.quietSamples;
    } else if (magnitude > highSilenceThreshold_m_s) {
        voice.quietSamples = 0;
    }
    if (voice.quietSamples < minimumQuietSamples_)
        return;

    voice.string.reset();
    voice.quietSamples = 0;
    voice.active = false;
    voice.heldNoteCount = 0;
    voice.sustained = false;
    if (diagnostics_.activeVoiceCount > 0)
        --diagnostics_.activeVoiceCount;
}

} // namespace aether
