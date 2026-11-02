#include "model/NoteParameters.h"

#include <algorithm>
#include <cmath>

namespace aether::model {
namespace {
constexpr double c4Frequency_Hz = 261.625565;
constexpr double c4Length_m = 0.65;
constexpr double stringDensity_kg_m = 0.0062;
constexpr double c4Inharmonicity = 0.0003;
constexpr double c4T60_s = 210.110086;
constexpr std::string_view profileSource = "synthetic_research:p03-estimated-curves-v1";
}

bool makeNoteParameters(std::uint8_t midiNote, const TuningConfiguration& tuning,
                        NoteParameters& result) noexcept {
    if (midiNote < firstMidiNote || midiNote > lastMidiNote
        || !std::isfinite(tuning.referenceA4_Hz)
        || tuning.referenceA4_Hz < 400.0 || tuning.referenceA4_Hz > 480.0
        || !std::isfinite(tuning.stretchCentsPerOctaveSquared)
        || std::abs(tuning.stretchCentsPerOctaveSquared) > 4.0
        || !std::isfinite(tuning.inharmonicityScale)
        || tuning.inharmonicityScale < 0.0 || tuning.inharmonicityScale > 4.0)
        return false;

    const double semitonesFromA4 = static_cast<double>(midiNote) - 69.0;
    const double octavesFromA4 = semitonesFromA4 / 12.0;
    const double stretchCents = tuning.stretchCentsPerOctaveSquared
        * octavesFromA4 * std::abs(octavesFromA4);
    const double frequency = tuning.referenceA4_Hz
        * std::exp2(semitonesFromA4 / 12.0 + stretchCents / 1200.0);
    if (!std::isfinite(frequency) || frequency <= 0.0)
        return false;

    const double octavesFromC4 = (static_cast<double>(midiNote) - 60.0) / 12.0;
    const double length = std::clamp(c4Length_m * c4Frequency_Hz / frequency, 0.10, 3.0);
    const double waveSpeed_m_s = 2.0 * length * frequency;
    const double tension_N = stringDensity_kg_m * waveSpeed_m_s * waveSpeed_m_s;
    const double inharmonicity = std::clamp(
        c4Inharmonicity * std::exp2(-0.5 * octavesFromC4)
            * tuning.inharmonicityScale,
        0.0, 0.01);
    const double t60 = c4T60_s * std::pow(0.75, std::abs(octavesFromC4));
    const double hammerMass_kg = 0.008 * std::exp2(-0.10 * octavesFromC4);
    const double hammerStiffness_N_m_p = 8.0e9 * std::exp2(0.25 * octavesFromC4);

    if (!std::isfinite(tension_N) || tension_N < 1.0 || tension_N > 5000.0
        || !std::isfinite(t60) || t60 < 0.1
        || !std::isfinite(hammerMass_kg) || !std::isfinite(hammerStiffness_N_m_p))
        return false;

    NoteParameters note;
    note.midiNote = midiNote;
    note.targetFrequency_Hz = frequency;
    note.calibrationStatus = CalibrationStatus::estimated;
    note.sourceRef = profileSource;
    note.string.length_m = length;
    note.string.linearDensity_kg_m = stringDensity_kg_m;
    note.string.tension_N = tension_N;
    note.string.fundamentalHz = frequency;
    note.string.inharmonicityB = inharmonicity;
    note.string.fundamentalT60Seconds = t60;
    note.string.lossCurvature = 0.08;
    note.string.damperT60Seconds = 0.8;
    note.string.hammerMass_kg = hammerMass_kg;
    note.string.hammerStiffness_N_m_p = hammerStiffness_N_m_p;
    note.string.hammerExponent_p = 2.5;
    note.string.strikePositionFraction = 0.12;
    note.string.readoutPositionFraction = 0.92;
    result = note;
    return true;
}

bool makeNoteParameterBank(const TuningConfiguration& tuning,
                           std::array<NoteParameters, midiNoteCount>& result) noexcept {
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto midiNote = static_cast<std::uint8_t>(firstMidiNote + index);
        if (!makeNoteParameters(midiNote, tuning, result[index]))
            return false;
    }
    return true;
}

} // namespace aether::model
