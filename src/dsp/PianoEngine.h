#pragma once
#include "dsp/string/WaveguideString.h"

#include <cstdint>
#include <span>

namespace aether {
struct MidiEvent { std::uint32_t sampleOffset; std::uint8_t status, data1, data2; };
class PianoEngine {
public:
    void prepare(double sampleRate, std::uint32_t maximumBlockSize,
                 const string::StringParameters& stringParameters = {}) noexcept;
    void reset() noexcept;
    void process(float* left, float* right, std::uint32_t samples,
                 std::span<const MidiEvent> events = {}) noexcept;
    void handleEvent(const MidiEvent& event) noexcept;
    [[nodiscard]] bool validEvents(std::span<const MidiEvent> events, std::uint32_t samples) const noexcept;
    [[nodiscard]] const string::WaveguideDiagnostics& diagnostics() const noexcept {
        return string_.diagnostics();
    }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] double hammerSpeedForVelocity(std::uint8_t velocity) const noexcept;
private:
    double sampleRate_ = 0.0;
    std::uint32_t maximumBlockSize_ = 0;
    string::WaveguideString string_;
    bool prepared_ = false;
};
}
