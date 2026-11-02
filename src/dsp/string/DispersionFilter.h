#pragma once

namespace aether::string {

// Two identical first-order allpass sections approximate stiff-string phase
// dispersion. The coefficient is calibrated to the second partial at prepare.
class DispersionFilter {
public:
    [[nodiscard]] bool prepare(double sampleRateHz, double fundamentalHz,
                               double inharmonicityB);
    void reset() noexcept;
    [[nodiscard]] double process(double input) noexcept;
    [[nodiscard]] double phaseAt(double radiansPerSample) const noexcept;
    [[nodiscard]] double magnitudeAt(double radiansPerSample) const noexcept;
    [[nodiscard]] double coefficient() const noexcept { return coefficient_; }
    [[nodiscard]] bool isStable() const noexcept;

private:
    [[nodiscard]] static double oneSectionPhase(double coefficient,
                                                double radiansPerSample) noexcept;

    double coefficient_ = 0.0;
    double x1_ = 0.0, y1_ = 0.0;
    double x2_ = 0.0, y2_ = 0.0;
    bool enabled_ = false;
    bool prepared_ = false;
};

} // namespace aether::string
