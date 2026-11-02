#pragma once

#include "dsp/string/WaveguideString.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aether::model {

inline constexpr std::uint8_t firstMidiNote = 21;
inline constexpr std::uint8_t lastMidiNote = 108;
inline constexpr std::size_t midiNoteCount = lastMidiNote - firstMidiNote + 1;

enum class CalibrationStatus : std::uint8_t {
    estimated,
    fitted,
    measured
};

struct TuningConfiguration {
    double referenceA4_Hz = 440.0;
    double stretchCentsPerOctaveSquared = 0.0;
    double inharmonicityScale = 1.0;
};

// Runtime-ready schema for one key. All physical values retain their SI units;
// sourceRef and calibrationStatus prevent synthetic curves being called measured.
struct NoteParameters {
    std::uint8_t midiNote = 60;
    double targetFrequency_Hz = 261.625565;
    string::StringParameters string{};
    CalibrationStatus calibrationStatus = CalibrationStatus::estimated;
    std::string_view sourceRef = "synthetic_research:p03-estimated-curves-v1";
};

[[nodiscard]] bool makeNoteParameters(std::uint8_t midiNote,
                                      const TuningConfiguration& tuning,
                                      NoteParameters& result) noexcept;

[[nodiscard]] bool makeNoteParameterBank(
    const TuningConfiguration& tuning,
    std::array<NoteParameters, midiNoteCount>& result) noexcept;

} // namespace aether::model
