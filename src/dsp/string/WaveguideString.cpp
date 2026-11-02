#include "dsp/string/WaveguideString.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace aether::string {

bool WaveguideString::prepare(double sampleRateHz, const StringParameters& parameters) {
    if (!std::isfinite(sampleRateHz) || sampleRateHz < 8000.0 || sampleRateHz > 192000.0
        || !std::isfinite(parameters.length_m) || parameters.length_m < 0.1 || parameters.length_m > 3.0
        || !std::isfinite(parameters.linearDensity_kg_m)
        || parameters.linearDensity_kg_m < 1.0e-4 || parameters.linearDensity_kg_m > 0.1
        || !std::isfinite(parameters.tension_N) || parameters.tension_N < 1.0 || parameters.tension_N > 5000.0
        || !std::isfinite(parameters.fundamentalHz) || parameters.fundamentalHz <= 0.0
        || parameters.fundamentalHz >= 0.25 * sampleRateHz
        || !std::isfinite(parameters.inharmonicityB) || parameters.inharmonicityB < 0.0
        || parameters.inharmonicityB > 0.01
        || !std::isfinite(parameters.damperT60Seconds) || parameters.damperT60Seconds < 0.05
        || parameters.damperT60Seconds > 10.0
        || !std::isfinite(parameters.strikePositionFraction)
        || parameters.strikePositionFraction <= 0.0 || parameters.strikePositionFraction >= 1.0
        || !std::isfinite(parameters.readoutPositionFraction)
        || parameters.readoutPositionFraction <= parameters.strikePositionFraction
        || parameters.readoutPositionFraction >= 1.0)
        return false;

    const double speed = std::sqrt(parameters.tension_N / parameters.linearDensity_kg_m);
    const double idealFundamental = speed / (2.0 * parameters.length_m);
    const double idealLoop = sampleRateHz / idealFundamental;
    const double omega0 = 2.0 * std::numbers::pi * parameters.fundamentalHz / sampleRateHz;
    if (!std::isfinite(speed) || !std::isfinite(idealLoop) || idealLoop < 8.0)
        return false;

    if (!dispersion_.prepare(sampleRateHz, parameters.fundamentalHz, parameters.inharmonicityB)
        || !loss_.prepare(sampleRateHz, parameters.fundamentalHz,
                          parameters.fundamentalT60Seconds, parameters.lossCurvature)
        || !hammer_.prepare(sampleRateHz, parameters.hammerMass_kg,
                            parameters.hammerStiffness_N_m_p, parameters.hammerExponent_p))
        return false;

    leftDelaySamples_ = idealLoop * parameters.strikePositionFraction;
    const double phaseBeforeRight = FractionalDelay::phaseForDelay(leftDelaySamples_, omega0)
                                  + loss_.phaseAt(omega0) + dispersion_.phaseAt(omega0);
    const double targetRightPhase = -2.0 * std::numbers::pi - phaseBeforeRight;
    double lowerDelay = 0.0;
    double upperDelay = 2.0 * idealLoop;
    if (FractionalDelay::phaseForDelay(upperDelay, omega0) > targetRightPhase)
        return false;
    for (int iteration = 0; iteration < 64; ++iteration) {
        const double middle = 0.5 * (lowerDelay + upperDelay);
        if (FractionalDelay::phaseForDelay(middle, omega0) > targetRightPhase)
            lowerDelay = middle;
        else
            upperDelay = middle;
    }
    rightDelaySamples_ = 0.5 * (lowerDelay + upperDelay);
    if (rightDelaySamples_ < 1.0)
        return false;

    const double directDelay = 0.5 * idealLoop
        * (parameters.readoutPositionFraction - parameters.strikePositionFraction);
    const double reflectedDelay = 0.5 * idealLoop
        * (2.0 - parameters.readoutPositionFraction - parameters.strikePositionFraction);
    if (leftDelaySamples_ < 1.0 || rightDelaySamples_ < 1.0
        || directDelay < 1.0 || reflectedDelay < 1.0)
        return false;
    // processSample reads each stored outgoing wave before calculating this
    // sample's outgoing value. That state handoff already contributes one
    // sample, so the explicit ring delay is the physical path delay minus one.
    if (!leftReturn_.prepare(leftDelaySamples_ - 1.0)
        || !rightReturn_.prepare(rightDelaySamples_ - 1.0)
        || !pickupDirect_.prepare(directDelay - 1.0)
        || !pickupReflected_.prepare(reflectedDelay - 1.0))
        return false;

    parameters_ = parameters;
    sampleRateHz_ = sampleRateHz;
    samplePeriod_s_ = 1.0 / sampleRateHz;
    impedance_Ns_m_ = std::sqrt(parameters.tension_N * parameters.linearDensity_kg_m);
    if (!std::isfinite(impedance_Ns_m_) || impedance_Ns_m_ <= 0.0)
        return false;
    damperLoopGain_ = std::pow(10.0, -3.0
        / (parameters.fundamentalHz * parameters.damperT60Seconds));
    prepared_ = true;
    reset();
    return true;
}

void WaveguideString::clearDynamicState() noexcept {
    leftReturn_.reset();
    rightReturn_.reset();
    pickupDirect_.reset();
    pickupReflected_.reset();
    dispersion_.reset();
    loss_.reset();
    hammer_.reset();
    displacementAtStrike_m_ = 0.0;
    leftOutgoing_m_s_ = 0.0;
    rightFilteredOutgoing_m_s_ = 0.0;
    lastInitialHammerSpeed_m_s_ = 0.0;
    damperOn_ = false;
    diagnostics_ = {};
}

void WaveguideString::reset() noexcept {
    if (prepared_)
        clearDynamicState();
}

bool WaveguideString::noteOn(double initialHammerSpeed_m_s) noexcept {
    if (!prepared_ || !hammer_.trigger(initialHammerSpeed_m_s, displacementAtStrike_m_))
        return false;
    lastInitialHammerSpeed_m_s_ = initialHammerSpeed_m_s;
    diagnostics_.lastInitialHammerEnergy_J = 0.5 * parameters_.hammerMass_kg
        * initialHammerSpeed_m_s * initialHammerSpeed_m_s;
    ++diagnostics_.noteOnCount;
    damperOn_ = false;
    return true;
}

void WaveguideString::noteOff() noexcept {
    setDamperEngaged(true);
}

void WaveguideString::setDamperEngaged(bool engaged) noexcept {
    if (prepared_)
        damperOn_ = engaged;
}

double WaveguideString::processSample() noexcept {
    if (!prepared_)
        return 0.0;

    const double incidentLeft = -leftReturn_.process(leftOutgoing_m_s_);
    const double incidentRight = -rightReturn_.process(rightFilteredOutgoing_m_s_);
    const auto hammerStep = hammer_.process(
        displacementAtStrike_m_, incidentLeft + incidentRight, impedance_Ns_m_);
    const double forceWave = hammerStep.force_N / (2.0 * impedance_Ns_m_);
    leftOutgoing_m_s_ = incidentRight + forceWave;
    const double rightOutgoing = incidentLeft + forceWave;
    const double throughLoss = loss_.process(rightOutgoing);
    const double throughDispersion = dispersion_.process(throughLoss);
    rightFilteredOutgoing_m_s_ = throughDispersion * (damperOn_ ? damperLoopGain_ : 1.0);
    displacementAtStrike_m_ += hammerStep.stringVelocity_m_s * samplePeriod_s_;

    const double direct = pickupDirect_.process(rightFilteredOutgoing_m_s_);
    const double reflected = -pickupReflected_.process(rightFilteredOutgoing_m_s_);
    const double output = direct + reflected;
    ++diagnostics_.samplesProcessed;
    diagnostics_.peakForce_N = std::max(diagnostics_.peakForce_N, hammerStep.force_N);
    diagnostics_.hammerImpulse_N_s = hammer_.impulse_N_s();
    diagnostics_.contactDuration_s = hammer_.contactDuration_s();
    if (hammerStep.inContact)
        ++diagnostics_.contactSamples;
    diagnostics_.lastHammerMomentumChange_N_s = parameters_.hammerMass_kg
        * (lastInitialHammerSpeed_m_s_ - hammer_.velocity_m_s());
    diagnostics_.maximumAbsoluteVelocity_m_s = std::max(
        diagnostics_.maximumAbsoluteVelocity_m_s, std::abs(output));

    if (!std::isfinite(output) || !std::isfinite(displacementAtStrike_m_)
        || !std::isfinite(leftOutgoing_m_s_) || !std::isfinite(rightFilteredOutgoing_m_s_)) {
        const auto failures = diagnostics_.nonFiniteStateCount + 1;
        clearDynamicState();
        diagnostics_.nonFiniteStateCount = failures;
        return 0.0;
    }
    return output;
}

} // namespace aether::string
