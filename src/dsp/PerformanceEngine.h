#pragma once

#include "common/MidiEvent.h"
#include "dsp/string/WaveguideString.h"
#include "model/NoteParameters.h"

#include <array>
#include <cstdint>
#include <span>

namespace aether {

struct PerformanceDiagnostics {
    std::uint32_t activeVoiceCount = 0;
    std::uint32_t peakActiveVoiceCount = 0;
    std::uint64_t acceptedNoteOns = 0;
    std::uint64_t acceptedNoteOffs = 0;
    std::uint64_t unmatchedNoteOffs = 0;
    std::uint64_t rejectedEvents = 0;
    std::uint64_t nonFiniteOutputs = 0;
    double peakOutputVelocity_m_s = 0.0;
};

// Fixed-capacity MIDI 21..108 performance engine. One persistent string state
// belongs to each pitch; repeated Note On retriggers its hammer without clearing
// the traveling waves. All allocation is confined to prepare().
class PerformanceEngine {
public:
    [[nodiscard]] bool prepare(double sampleRateHz, std::uint32_t maximumBlockSize,
                               const model::TuningConfiguration& tuning = {}) noexcept;
    void reset() noexcept;
    void process(float* left, float* right, std::uint32_t samples,
                 std::span<const MidiEvent> events = {}) noexcept;
    void handleEvent(const MidiEvent& event) noexcept;

    [[nodiscard]] bool validEvents(std::span<const MidiEvent> events,
                                   std::uint32_t samples) const noexcept;
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] double sampleRateHz() const noexcept { return sampleRateHz_; }
    [[nodiscard]] const PerformanceDiagnostics& diagnostics() const noexcept {
        return diagnostics_;
    }
    [[nodiscard]] const model::NoteParameters* noteParameters(std::uint8_t midiNote) const noexcept;
    [[nodiscard]] double hammerSpeedForVelocity(std::uint8_t velocity) const noexcept;

private:
    struct Voice {
        string::WaveguideString string;
        std::uint32_t quietSamples = 0;
        std::uint8_t heldNoteCount = 0;
        bool active = false;
        bool sustained = false;
    };

    [[nodiscard]] bool noteOn(std::uint8_t midiNote, std::uint8_t velocity) noexcept;
    void noteOff(std::uint8_t midiNote) noexcept;
    void setDamper(Voice& voice, bool engaged) noexcept;
    void updateSilence(Voice& voice, double outputVelocity_m_s) noexcept;

    std::array<Voice, model::midiNoteCount> voices_{};
    std::array<model::NoteParameters, model::midiNoteCount> notes_{};
    PerformanceDiagnostics diagnostics_{};
    double sampleRateHz_ = 0.0;
    std::uint32_t minimumQuietSamples_ = 0;
    bool sustainPedalDown_ = false;
    bool prepared_ = false;
};

} // namespace aether
