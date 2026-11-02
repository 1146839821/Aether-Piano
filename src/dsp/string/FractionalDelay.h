#pragma once

#include <cstddef>
#include <vector>

namespace aether::string {

// Fixed, prepare-time delay: integer samples in a ring and a first-order
// Thiran allpass for the fractional part. process() performs no allocation.
class FractionalDelay {
public:
    [[nodiscard]] bool prepare(double delaySamples);
    void reset() noexcept;
    [[nodiscard]] double process(double input) noexcept;
    [[nodiscard]] double phaseAt(double radiansPerSample) const noexcept;
    [[nodiscard]] static double phaseForDelay(double delaySamples,
                                              double radiansPerSample) noexcept;

    [[nodiscard]] double delaySamples() const noexcept { return delaySamples_; }
    [[nodiscard]] double fractionalCoefficient() const noexcept { return coefficient_; }
    [[nodiscard]] bool isPrepared() const noexcept { return !buffer_.empty(); }

private:
    std::vector<double> buffer_;
    std::size_t writeIndex_ = 0;
    std::size_t integerDelay_ = 0;
    double delaySamples_ = 0.0;
    double coefficient_ = 1.0;
    double previousInput_ = 0.0;
    double previousOutput_ = 0.0;
    bool hasFractionalPart_ = false;
};

} // namespace aether::string
