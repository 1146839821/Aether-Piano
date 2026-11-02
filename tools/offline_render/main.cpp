#include "dsp/PianoEngine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

static void u16(std::ostream& out, std::uint16_t value) {
    out.put(static_cast<char>(value));
    out.put(static_cast<char>(value >> 8));
}

static void u32(std::ostream& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i)
        out.put(static_cast<char>(value >> (8 * i)));
}

int main(int argc, char** argv) {
    std::string path = "render.wav";
    double seconds = 1.0;
    double noteOffSeconds = 0.8;
    std::uint32_t sampleRate = 48000;
    int velocity = 80;
    double inharmonicityB = 0.0003;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) path = argv[++i];
        else if (arg == "--seconds" && i + 1 < argc) seconds = std::stod(argv[++i]);
        else if (arg == "--sample-rate" && i + 1 < argc) sampleRate = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        else if (arg == "--velocity" && i + 1 < argc) velocity = std::stoi(argv[++i]);
        else if (arg == "--note-off" && i + 1 < argc) noteOffSeconds = std::stod(argv[++i]);
        else if (arg == "--inharmonicity" && i + 1 < argc) inharmonicityB = std::stod(argv[++i]);
        else {
            std::cerr << "Usage: offline_render [--output file.wav] [--seconds duration] "
                         "[--sample-rate 44100|48000|96000] [--velocity 1..127] "
                         "[--note-off seconds] [--inharmonicity B]\n";
            return 2;
        }
    }

    constexpr std::uint32_t channels = 2;
    constexpr std::uint32_t bytesPerSample = 4;
    if (!std::isfinite(seconds) || seconds <= 0.0 || seconds > 3600.0
        || (sampleRate != 44100 && sampleRate != 48000 && sampleRate != 96000)
        || velocity < 1 || velocity > 127
        || !std::isfinite(noteOffSeconds) || noteOffSeconds < 0.0) {
        std::cerr << "invalid duration, supported sample rate, velocity, or note-off time\n";
        return 2;
    }
    const auto frameCount64 = static_cast<std::uint64_t>(seconds * sampleRate);
    const std::uint64_t dataBytes64 = frameCount64 * channels * bytesPerSample;
    if (frameCount64 == 0 || frameCount64 > std::numeric_limits<std::uint32_t>::max()
        || dataBytes64 > std::numeric_limits<std::uint32_t>::max() - 36) {
        std::cerr << "render exceeds RIFF/WAV size limits\n";
        return 2;
    }
    const auto frames = static_cast<std::uint32_t>(frameCount64);
    const auto noteOffFrame = static_cast<std::uint64_t>(noteOffSeconds * sampleRate);

    aether::PianoEngine engine;
    aether::string::StringParameters stringParameters;
    stringParameters.inharmonicityB = inharmonicityB;
    engine.prepare(sampleRate, 512, stringParameters);
    if (!engine.isPrepared()) {
        std::cerr << "failed to prepare C4 engine\n";
        return 1;
    }
    std::vector<float> left(512), right(512);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "cannot open output: " << path << '\n';
        return 1;
    }
    out.write("RIFF", 4); u32(out, 36 + static_cast<std::uint32_t>(dataBytes64));
    out.write("WAVEfmt ", 8); u32(out, 16); u16(out, 3); u16(out, channels);
    u32(out, sampleRate); u32(out, sampleRate * channels * bytesPerSample);
    u16(out, channels * bytesPerSample); u16(out, 8 * bytesPerSample);
    out.write("data", 4); u32(out, static_cast<std::uint32_t>(dataBytes64));

    double peak = 0.0;
    for (std::uint32_t done = 0; done < frames;) {
        const auto count = std::min<std::uint32_t>(512, frames - done);
        std::array<aether::MidiEvent, 2> blockEvents{};
        std::size_t eventCount = 0;
        if (done == 0)
            blockEvents[eventCount++] = {0, 0x90, 60, static_cast<std::uint8_t>(velocity)};
        if (noteOffFrame < frames && noteOffFrame >= done
            && noteOffFrame < static_cast<std::uint64_t>(done) + count) {
            blockEvents[eventCount++] = {
                static_cast<std::uint32_t>(noteOffFrame - done), 0x80, 60, 0};
        }
        engine.process(left.data(), right.data(), count,
                       std::span<const aether::MidiEvent>(blockEvents.data(), eventCount));
        for (std::uint32_t i = 0; i < count; ++i) {
            peak = std::max(peak, std::abs(static_cast<double>(left[i])));
            u32(out, std::bit_cast<std::uint32_t>(left[i]));
            u32(out, std::bit_cast<std::uint32_t>(right[i]));
        }
        done += count;
    }
    if (!out) {
        std::cerr << "write failed\n";
        return 1;
    }
    const auto& diagnostic = engine.diagnostics();
    std::cout << "sample_rate_hz=" << sampleRate << " channels=2 bits=32 format=float"
              << " samples_per_channel=" << frames << " midi_note=60 velocity=" << velocity
              << " inharmonicity_B=" << inharmonicityB
              << " hammer_speed_m_s=" << engine.hammerSpeedForVelocity(
                    static_cast<std::uint8_t>(velocity))
              << " initial_hammer_energy_J=" << diagnostic.lastInitialHammerEnergy_J
              << " peak_force_N=" << diagnostic.peakForce_N
              << " hammer_impulse_N_s=" << diagnostic.hammerImpulse_N_s
              << " hammer_momentum_change_N_s=" << diagnostic.lastHammerMomentumChange_N_s
              << " contact_duration_s=" << diagnostic.contactDuration_s
              << " output_peak_m_s=" << peak
              << " non_finite_states=" << diagnostic.nonFiniteStateCount
              << " output=" << path << '\n';
}
