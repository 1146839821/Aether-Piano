#pragma once
#include <cstdint>
#include <span>

namespace aether {
struct MidiEvent { std::uint32_t sampleOffset; std::uint8_t status, data1, data2; };
class PianoEngine {
public:
    void prepare(double sampleRate, std::uint32_t maximumBlockSize) noexcept;
    void reset() noexcept;
    void process(float* left, float* right, std::uint32_t samples,
                 std::span<const MidiEvent> events = {}) noexcept;
    [[nodiscard]] bool validEvents(std::span<const MidiEvent> events, std::uint32_t samples) const noexcept;
private:
    double sampleRate_ = 0.0;
    std::uint32_t maximumBlockSize_ = 0;
};
}
