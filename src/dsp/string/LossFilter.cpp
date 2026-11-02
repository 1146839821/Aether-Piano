#include "dsp/string/LossFilter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace aether::string {

bool LossFilter::prepare(double sampleRateHz, double fundamentalHz,
                         double fundamentalT60Seconds, double curvature) {
    if (!std::isfinite(sampleRateHz) || sampleRateHz < 8000.0 || sampleRateHz > 192000.0
        || !std::isfinite(fundamentalHz) || fundamentalHz <= 0.0
        || fundamentalHz >= 0.5 * sampleRateHz
        || !std::isfinite(fundamentalT60Seconds) || fundamentalT60Seconds < 0.1
        || fundamentalT60Seconds > 1.0e6
        || !std::isfinite(curvature) || curvature < 0.0 || curvature > 0.25)
        return false;

    const double omega = 2.0 * std::numbers::pi * fundamentalHz / sampleRateHz;
    targetMagnitude_ = std::pow(10.0, -3.0 / (fundamentalHz * fundamentalT60Seconds));
    const double sinHalf = std::sin(0.5 * omega);
    const double denominator = 4.0 * sinHalf * sinHalf;
    const double maximumCurvature = denominator > 0.0
        ? std::clamp((1.0 - targetMagnitude_) / denominator, 0.0, 0.25)
        : 0.0;
    curvature_ = std::min(curvature, maximumCurvature);
    const double shapeAtFundamental = 1.0 - denominator * curvature_;
    const double gain = targetMagnitude_ / shapeAtFundamental;
    if (!std::isfinite(gain) || gain < 0.0 || gain > 1.0 + 1.0e-12)
        return false;

    coefficients_[0] = gain * curvature_;
    coefficients_[1] = gain * (1.0 - 2.0 * curvature_);
    coefficients_[2] = gain * curvature_;
    prepared_ = true;
    reset();
    return true;
}

void LossFilter::reset() noexcept {
    previous1_ = 0.0;
    previous2_ = 0.0;
}

double LossFilter::process(double input) noexcept {
    if (!prepared_)
        return 0.0;
    const double output = coefficients_[0] * input
                        + coefficients_[1] * previous1_
                        + coefficients_[2] * previous2_;
    previous2_ = previous1_;
    previous1_ = input;
    return output;
}

double LossFilter::magnitudeAt(double radiansPerSample) const noexcept {
    if (!prepared_ || !std::isfinite(radiansPerSample)
        || radiansPerSample < 0.0 || radiansPerSample > std::numbers::pi)
        return 0.0;
    const double shape = 1.0 - 4.0 * curvature_
                               * std::sin(0.5 * radiansPerSample)
                               * std::sin(0.5 * radiansPerSample);
    return (coefficients_[0] + coefficients_[1] + coefficients_[2]) * shape;
}

double LossFilter::phaseAt(double radiansPerSample) const noexcept {
    if (!prepared_ || !std::isfinite(radiansPerSample)
        || radiansPerSample < 0.0 || radiansPerSample > std::numbers::pi)
        return 0.0;
    return -radiansPerSample;
}

} // namespace aether::string
