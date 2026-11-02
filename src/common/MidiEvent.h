#pragma once

#include <cstdint>

namespace aether {

struct MidiEvent {
    std::uint32_t sampleOffset = 0;
    std::uint8_t status = 0;
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;
};

} // namespace aether
