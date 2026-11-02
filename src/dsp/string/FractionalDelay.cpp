#include "dsp/string/FractionalDelay.h"

#include <cmath>
#include <limits>
#include <numbers>

namespace aether::string {

bool FractionalDelay::prepare(double delaySamples) {
    constexpr double maxDelaySamples = 1'000'000.0;
    if (!std::isfinite(delaySamples) || delaySamples < 0.0 || delaySamples > maxDelaySamples)
        return false;

    const auto integerDelay = static_cast<std::size_t>(std::floor(delaySamples));
    std::vector<double> newBuffer(integerDelay + 1, 0.0);
    buffer_.swap(newBuffer);
    integerDelay_ = integerDelay;
    delaySamples_ = delaySamples;
    const double fraction = delaySamples - static_cast<double>(integerDelay);
    hasFractionalPart_ = fraction > 1.0e-12;
    coefficient_ = hasFractionalPart_ ? (1.0 - fraction) / (1.0 + fraction) : 1.0;
    reset();
    return true;
}

void FractionalDelay::reset() noexcept {
    for (double& value : buffer_)
        value = 0.0;
    writeIndex_ = 0;
    previousInput_ = 0.0;
    previousOutput_ = 0.0;
}

double FractionalDelay::process(double input) noexcept {
    if (buffer_.empty())
        return 0.0;

    buffer_[writeIndex_] = input;
    const std::size_t size = buffer_.size();
    const std::size_t readIndex =
        (writeIndex_ + size - (integerDelay_ % size)) % size;
    const double integerDelayed = buffer_[readIndex];
    writeIndex_ = (writeIndex_ + 1) % size;

    if (!hasFractionalPart_)
        return integerDelayed;

    // H(z)=(a+z^-1)/(1+a*z^-1), |a|<1. Its DC group delay is fraction.
    const double output = coefficient_ * integerDelayed + previousInput_
                        - coefficient_ * previousOutput_;
    previousInput_ = integerDelayed;
    previousOutput_ = output;
    return output;
}

double FractionalDelay::phaseForDelay(double delaySamples,
                                      double radiansPerSample) noexcept {
    if (!std::isfinite(delaySamples) || delaySamples < 0.0
        || !std::isfinite(radiansPerSample) || radiansPerSample < 0.0
        || radiansPerSample > std::numbers::pi)
        return 0.0;
    const double integer = std::floor(delaySamples);
    const double fraction = delaySamples - integer;
    double phase = -radiansPerSample * integer;
    if (fraction > 1.0e-12) {
        const double a = (1.0 - fraction) / (1.0 + fraction);
        phase += -2.0 * std::atan2(
            (1.0 - a) * std::sin(0.5 * radiansPerSample),
            (1.0 + a) * std::cos(0.5 * radiansPerSample));
    }
    return phase;
}

double FractionalDelay::phaseAt(double radiansPerSample) const noexcept {
    return buffer_.empty() ? 0.0 : phaseForDelay(delaySamples_, radiansPerSample);
}

} // namespace aether::string
