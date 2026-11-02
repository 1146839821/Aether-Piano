#include "dsp/PianoEngine.h"

namespace aether {
void PianoEngine::prepare(double sampleRate, std::uint32_t maximumBlockSize) noexcept {
    sampleRate_ = sampleRate;
    maximumBlockSize_ = maximumBlockSize;
}
void PianoEngine::reset() noexcept {}
void PianoEngine::process(float* left, float* right, std::uint32_t samples,
                          std::span<const MidiEvent>) noexcept {
    if (left == nullptr || right == nullptr) return;
    for (std::uint32_t i = 0; i < samples; ++i) left[i] = right[i] = 0.0f;
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
