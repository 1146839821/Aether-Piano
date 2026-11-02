#pragma once

#include <array>

namespace aether::string {

// Positive, symmetric three-tap FIR. Its phase is exactly one sample and its
// magnitude never exceeds one; the low-frequency T60 is specified at prepare.
class LossFilter {
public:
    [[nodiscard]] bool prepare(double sampleRateHz, double fundamentalHz,
                               double fundamentalT60Seconds, double curvature);
    void reset() noexcept;
    [[nodiscard]] double process(double input) noexcept;
    [[nodiscard]] double magnitudeAt(double radiansPerSample) const noexcept;
    [[nodiscard]] double phaseAt(double radiansPerSample) const noexcept;
    [[nodiscard]] double targetFundamentalMagnitude() const noexcept { return targetMagnitude_; }
    [[nodiscard]] double curvature() const noexcept { return curvature_; }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }

private:
    std::array<double, 3> coefficients_{};
    double previous1_ = 0.0;
    double previous2_ = 0.0;
    double targetMagnitude_ = 1.0;
    double curvature_ = 0.0;
    bool prepared_ = false;
};

} // namespace aether::string
