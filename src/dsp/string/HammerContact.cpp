#include "dsp/string/HammerContact.h"

#include <algorithm>
#include <cmath>

namespace aether::string {

bool HammerContact::prepare(double sampleRateHz, double massKg,
                            double stiffnessNPerMeterPowerP, double exponentP) noexcept {
    if (!std::isfinite(sampleRateHz) || sampleRateHz < 8000.0 || sampleRateHz > 192000.0
        || !std::isfinite(massKg) || massKg < 5.0e-4 || massKg > 0.1
        || !std::isfinite(stiffnessNPerMeterPowerP)
        || stiffnessNPerMeterPowerP < 1.0e5 || stiffnessNPerMeterPowerP > 1.0e12
        || !std::isfinite(exponentP) || exponentP < 1.0 || exponentP > 4.0)
        return false;
    sampleRateHz_ = sampleRateHz;
    massKg_ = massKg;
    stiffness_ = stiffnessNPerMeterPowerP;
    exponent_ = exponentP;
    prepared_ = true;
    reset();
    return true;
}

void HammerContact::reset() noexcept {
    position_m_ = 0.0;
    velocity_m_s_ = 0.0;
    peakForce_N_ = 0.0;
    impulse_N_s_ = 0.0;
    contactDuration_s_ = 0.0;
    inContact_ = false;
}

bool HammerContact::trigger(double initialVelocity_m_s,
                            double stringDisplacement_m) noexcept {
    if (!prepared_ || !std::isfinite(initialVelocity_m_s)
        || initialVelocity_m_s < 0.0 || initialVelocity_m_s > 5.0
        || !std::isfinite(stringDisplacement_m)) {
        inContact_ = false;
        return false;
    }
    position_m_ = stringDisplacement_m;
    velocity_m_s_ = initialVelocity_m_s;
    peakForce_N_ = 0.0;
    impulse_N_s_ = 0.0;
    contactDuration_s_ = 0.0;
    inContact_ = initialVelocity_m_s > 0.0;
    return true;
}

HammerContactStep HammerContact::process(double stringDisplacement_m,
                                         double incidentStringVelocity_m_s,
                                         double characteristicImpedance_Ns_m) noexcept {
    HammerContactStep result{0.0, incidentStringVelocity_m_s, position_m_, velocity_m_s_, inContact_};
    if (!prepared_ || !std::isfinite(stringDisplacement_m)
        || !std::isfinite(incidentStringVelocity_m_s)
        || !std::isfinite(characteristicImpedance_Ns_m)
        || characteristicImpedance_Ns_m <= 0.0) {
        inContact_ = false;
        result.inContact = false;
        return result;
    }

    const double dt = 1.0 / sampleRateHz_;
    if (!inContact_) {
        position_m_ += velocity_m_s_ * dt;
        result.hammerPosition_m = position_m_;
        return result;
    }

    const double compressionWithoutForce = position_m_ + velocity_m_s_ * dt
        - stringDisplacement_m - incidentStringVelocity_m_s * dt;
    if (!(compressionWithoutForce > 0.0) || !std::isfinite(compressionWithoutForce)) {
        position_m_ += velocity_m_s_ * dt;
        inContact_ = false;
        result.hammerPosition_m = position_m_;
        result.hammerVelocity_m_s = velocity_m_s_;
        result.inContact = false;
        return result;
    }

    const double compliancePerForce = dt * dt
        * (1.0 / massKg_ + 1.0 / (2.0 * characteristicImpedance_Ns_m));
    double lower = 0.0;
    double upper = stiffness_ * std::pow(compressionWithoutForce, exponent_);
    if (!std::isfinite(upper)) {
        inContact_ = false;
        result.inContact = false;
        return result;
    }
    for (int iteration = 0; iteration < 32; ++iteration) {
        const double middle = 0.5 * (lower + upper);
        const double compression = std::max(0.0, compressionWithoutForce - compliancePerForce * middle);
        const double lawForce = stiffness_ * std::pow(compression, exponent_);
        if (middle < lawForce)
            lower = middle;
        else
            upper = middle;
    }
    const double force = 0.5 * (lower + upper);
    const double nextHammerVelocity = velocity_m_s_ - force * dt / massKg_;
    const double stringVelocity = incidentStringVelocity_m_s
        + force / (2.0 * characteristicImpedance_Ns_m);
    position_m_ += nextHammerVelocity * dt;
    velocity_m_s_ = nextHammerVelocity;
    peakForce_N_ = force > peakForce_N_ ? force : peakForce_N_;
    impulse_N_s_ += force * dt;
    contactDuration_s_ += dt;

    result.force_N = force;
    result.stringVelocity_m_s = stringVelocity;
    result.hammerPosition_m = position_m_;
    result.hammerVelocity_m_s = velocity_m_s_;
    result.inContact = true;
    return result;
}

} // namespace aether::string
