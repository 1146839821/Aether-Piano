#pragma once

namespace aether::string {

struct HammerContactStep {
    double force_N = 0.0;
    double stringVelocity_m_s = 0.0;
    double hammerPosition_m = 0.0;
    double hammerVelocity_m_s = 0.0;
    bool inContact = false;
};

// Lumped hammer + memoryless felt law. A monotone scalar implicit solve
// evaluates F=K*max(0,delta)^p at the end of the sample interval.
class HammerContact {
public:
    [[nodiscard]] bool prepare(double sampleRateHz, double massKg,
                               double stiffnessNPerMeterPowerP, double exponentP) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool trigger(double initialVelocity_m_s,
                               double stringDisplacement_m) noexcept;
    [[nodiscard]] HammerContactStep process(double stringDisplacement_m,
                                            double incidentStringVelocity_m_s,
                                            double characteristicImpedance_Ns_m) noexcept;

    [[nodiscard]] bool isInContact() const noexcept { return inContact_; }
    [[nodiscard]] double velocity_m_s() const noexcept { return velocity_m_s_; }
    [[nodiscard]] double position_m() const noexcept { return position_m_; }
    [[nodiscard]] double peakForce_N() const noexcept { return peakForce_N_; }
    [[nodiscard]] double impulse_N_s() const noexcept { return impulse_N_s_; }
    [[nodiscard]] double contactDuration_s() const noexcept { return contactDuration_s_; }
    [[nodiscard]] double mass_kg() const noexcept { return massKg_; }

private:
    double sampleRateHz_ = 0.0;
    double massKg_ = 0.0;
    double stiffness_ = 0.0;
    double exponent_ = 0.0;
    double position_m_ = 0.0;
    double velocity_m_s_ = 0.0;
    double peakForce_N_ = 0.0;
    double impulse_N_s_ = 0.0;
    double contactDuration_s_ = 0.0;
    bool prepared_ = false;
    bool inContact_ = false;
};

} // namespace aether::string
