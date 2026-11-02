#include "dsp/string/DispersionFilter.h"

#include <cmath>
#include <numbers>

namespace aether::string {

double DispersionFilter::oneSectionPhase(double coefficient,
                                         double radiansPerSample) noexcept {
    return -2.0 * std::atan2(
        (1.0 - coefficient) * std::sin(0.5 * radiansPerSample),
        (1.0 + coefficient) * std::cos(0.5 * radiansPerSample));
}

bool DispersionFilter::prepare(double sampleRateHz, double fundamentalHz,
                               double inharmonicityB) {
    if (!std::isfinite(sampleRateHz) || sampleRateHz < 8000.0 || sampleRateHz > 192000.0
        || !std::isfinite(fundamentalHz) || fundamentalHz <= 0.0
        || fundamentalHz >= 0.25 * sampleRateHz
        || !std::isfinite(inharmonicityB) || inharmonicityB < 0.0
        || inharmonicityB > 0.01)
        return false;

    coefficient_ = 0.0;
    enabled_ = inharmonicityB >= 1.0e-8;
    if (enabled_) {
        const double secondHz = 2.0 * fundamentalHz
            * std::sqrt((1.0 + 4.0 * inharmonicityB) / (1.0 + inharmonicityB));
        if (secondHz >= 0.5 * sampleRateHz)
            return false;
        const double omega1 = 2.0 * std::numbers::pi * fundamentalHz / sampleRateHz;
        const double omega2 = 2.0 * std::numbers::pi * secondHz / sampleRateHz;
        const double ratio = secondHz / fundamentalHz;
        const auto phaseError = [&](double a) noexcept {
            const double phase1 = 2.0 * oneSectionPhase(a, omega1);
            const double phase2 = 2.0 * oneSectionPhase(a, omega2);
            return 2.0 * std::numbers::pi * (ratio - 2.0)
                 + ratio * phase1 - phase2;
        };

        // Higher sample rates lower the normalized frequency of low piano
        // notes, so the same partial stretch requires a pole closer to DC.
        // Keep a 1e-3 stability margin while allowing the 96 kHz A0 solution.
        double lower = -0.999;
        double upper = 0.0;
        double lowerError = phaseError(lower);
        double upperError = phaseError(upper);
        if (!(lowerError <= 0.0 && upperError >= 0.0))
            return false;
        for (int i = 0; i < 64; ++i) {
            const double middle = 0.5 * (lower + upper);
            if (phaseError(middle) > 0.0)
                upper = middle;
            else
                lower = middle;
        }
        coefficient_ = 0.5 * (lower + upper);
    }
    prepared_ = true;
    reset();
    return true;
}

void DispersionFilter::reset() noexcept {
    x1_ = y1_ = x2_ = y2_ = 0.0;
}

double DispersionFilter::process(double input) noexcept {
    if (!prepared_ || !enabled_)
        return prepared_ ? input : 0.0;
    const double first = coefficient_ * input + x1_ - coefficient_ * y1_;
    x1_ = input;
    y1_ = first;
    const double output = coefficient_ * first + x2_ - coefficient_ * y2_;
    x2_ = first;
    y2_ = output;
    return output;
}

double DispersionFilter::phaseAt(double radiansPerSample) const noexcept {
    if (!prepared_ || !std::isfinite(radiansPerSample)
        || radiansPerSample < 0.0 || radiansPerSample > std::numbers::pi)
        return 0.0;
    return enabled_ ? 2.0 * oneSectionPhase(coefficient_, radiansPerSample) : 0.0;
}

double DispersionFilter::magnitudeAt(double radiansPerSample) const noexcept {
    if (!prepared_ || !std::isfinite(radiansPerSample)
        || radiansPerSample < 0.0 || radiansPerSample > std::numbers::pi)
        return 0.0;
    return 1.0;
}

bool DispersionFilter::isStable() const noexcept {
    return prepared_ && (!enabled_ || std::abs(coefficient_) < 1.0);
}

} // namespace aether::string
