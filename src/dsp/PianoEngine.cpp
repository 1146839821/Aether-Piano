#include "dsp/PianoEngine.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace aether {
void PianoEngine::prepare(double sampleRate, std::uint32_t maximumBlockSize,
                          const string::StringParameters& stringParameters) noexcept {
    prepared_ = false;
    sampleRate_ = 0.0;
    maximumBlockSize_ = 0;
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 192000.0
        || maximumBlockSize == 0)
        return;

    try {
        if (!string_.prepare(sampleRate, stringParameters))
            return;
        sampleRate_ = sampleRate;
        maximumBlockSize_ = maximumBlockSize;
        prepared_ = true;
    } catch (...) {
        string_.reset();
    }
}
void PianoEngine::reset() noexcept {
    string_.reset();
}
void PianoEngine::process(float* left, float* right, std::uint32_t samples,
                          std::span<const MidiEvent> events) noexcept {
    if (left == nullptr || right == nullptr) return;
    if (samples > maximumBlockSize_ || !prepared_ || !validEvents(events, samples)) {
        for (std::uint32_t i = 0; i < samples; ++i) left[i] = right[i] = 0.0f;
        return;
    }

    std::size_t eventIndex = 0;
    for (std::uint32_t i = 0; i < samples; ++i) {
        while (eventIndex < events.size() && events[eventIndex].sampleOffset == i)
            handleEvent(events[eventIndex++]);
        const float output = static_cast<float>(string_.processSample());
        left[i] = output;
        right[i] = output;
    }
}
void PianoEngine::handleEvent(const MidiEvent& event) noexcept {
    if (!prepared_ || event.data1 != 60)
        return;
    const auto message = event.status & 0xf0;
    if (message == 0x90 && event.data2 != 0)
        (void) string_.noteOn(hammerSpeedForVelocity(event.data2));
    else if (message == 0x80 || (message == 0x90 && event.data2 == 0))
        string_.noteOff();
}

double PianoEngine::hammerSpeedForVelocity(std::uint8_t velocity) const noexcept {
    struct Anchor { double velocity; double speed; };
    constexpr std::array<Anchor, 5> anchors{{
        {1.0, 0.1}, {24.0, 0.3}, {64.0, 0.8}, {112.0, 1.6}, {127.0, 1.9}
    }};
    const double value = std::clamp<double>(velocity, 1.0, 127.0);
    for (std::size_t i = 1; i < anchors.size(); ++i) {
        if (value <= anchors[i].velocity) {
            const auto& a = anchors[i - 1];
            const auto& b = anchors[i];
            const double fraction = (value - a.velocity) / (b.velocity - a.velocity);
            return a.speed + fraction * (b.speed - a.speed);
        }
    }
    return anchors.back().speed;
}

bool PianoEngine::validEvents(std::span<const MidiEvent> events, std::uint32_t samples) const noexcept {
    if (samples > maximumBlockSize_) return false;
    std::uint32_t previous = 0;
    bool first = true;
    for (const auto& event : events) {
        if (event.sampleOffset >= samples || (!first && event.sampleOffset < previous)) return false;
        previous = event.sampleOffset;
        first = false;
    }
    return true;
}
}
