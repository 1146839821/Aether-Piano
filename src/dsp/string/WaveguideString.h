#pragma once

#include "dsp/string/DispersionFilter.h"
#include "dsp/string/FractionalDelay.h"
#include "dsp/string/HammerContact.h"
#include "dsp/string/LossFilter.h"

#include <cstdint>

namespace aether::string {

struct StringParameters {
    double length_m = 0.65;
    double linearDensity_kg_m = 0.0062;
    double tension_N = 716.9823814342922;
    double fundamentalHz = 261.625565;
    double inharmonicityB = 0.0003;
    double fundamentalT60Seconds = 210.110086;
    double lossCurvature = 0.08;
    double damperT60Seconds = 0.8;
    double hammerMass_kg = 0.008;
    double hammerStiffness_N_m_p = 8.0e9;
    double hammerExponent_p = 2.5;
    double strikePositionFraction = 0.12;
    double readoutPositionFraction = 0.92;
};

struct WaveguideDiagnostics {
    double peakForce_N = 0.0;
    double hammerImpulse_N_s = 0.0;
    double contactDuration_s = 0.0;
    double lastInitialHammerEnergy_J = 0.0;
    double lastHammerMomentumChange_N_s = 0.0;
    double maximumAbsoluteVelocity_m_s = 0.0;
    std::uint64_t samplesProcessed = 0;
    std::uint64_t contactSamples = 0;
    std::uint64_t noteOnCount = 0;
    std::uint64_t nonFiniteStateCount = 0;
};

class WaveguideString {
public:
    [[nodiscard]] bool prepare(double sampleRateHz, const StringParameters& parameters);
    void reset() noexcept;
    [[nodiscard]] bool noteOn(double initialHammerSpeed_m_s) noexcept;
    void noteOff() noexcept;
    void setDamperEngaged(bool engaged) noexcept;
    [[nodiscard]] double processSample() noexcept;

    [[nodiscard]] const WaveguideDiagnostics& diagnostics() const noexcept { return diagnostics_; }
    [[nodiscard]] double sampleRateHz() const noexcept { return sampleRateHz_; }
    [[nodiscard]] double characteristicImpedance_Ns_m() const noexcept { return impedance_Ns_m_; }
    [[nodiscard]] double tunedLoopDelaySamples() const noexcept { return leftDelaySamples_ + rightDelaySamples_; }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }

private:
    void clearDynamicState() noexcept;

    StringParameters parameters_{};
    FractionalDelay leftReturn_, rightReturn_, pickupDirect_, pickupReflected_;
    DispersionFilter dispersion_;
    LossFilter loss_;
    HammerContact hammer_;
    WaveguideDiagnostics diagnostics_{};
    double sampleRateHz_ = 0.0;
    double samplePeriod_s_ = 0.0;
    double impedance_Ns_m_ = 0.0;
    double displacementAtStrike_m_ = 0.0;
    double leftOutgoing_m_s_ = 0.0;
    double rightFilteredOutgoing_m_s_ = 0.0;
    double leftDelaySamples_ = 0.0;
    double rightDelaySamples_ = 0.0;
    double lastInitialHammerSpeed_m_s_ = 0.0;
    double damperLoopGain_ = 1.0;
    bool damperOn_ = false;
    bool prepared_ = false;
};

} // namespace aether::string
