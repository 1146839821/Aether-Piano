#include "dsp/PerformanceEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>

int main() {
    constexpr std::uint32_t sampleRate = 48000;
    constexpr std::uint32_t audioSeconds = 3;
    constexpr std::uint32_t totalSamples = sampleRate * audioSeconds;
    constexpr std::array<std::uint32_t, 4> voiceCounts{{1, 8, 32, 64}};
    constexpr std::array<std::uint32_t, 2> blockSizes{{64, 128}};
    constexpr std::size_t maximumBlocks = totalSamples / blockSizes.front();
    std::array<double, maximumBlocks> durations_ms{};
    std::array<float, 128> left{}, right{};

    for (const auto blockSize : blockSizes) {
        for (const auto noteCount : voiceCounts) {
            aether::PerformanceEngine engine;
            if (!engine.prepare(sampleRate, blockSize)) {
                std::cerr << "failed to prepare 88-note engine\n";
                return 1;
            }
            for (std::uint32_t index = 0; index < noteCount; ++index) {
                const auto note = static_cast<std::uint8_t>(21 + index);
                engine.handleEvent({0, 0x90, note, 80});
            }
            const std::uint32_t blocks = totalSamples / blockSize;
            for (std::uint32_t block = 0; block < blocks; ++block) {
                const auto done = block * blockSize;
                if (done == sampleRate * 2) {
                    for (std::uint32_t index = 0; index < noteCount; ++index) {
                        const auto note = static_cast<std::uint8_t>(21 + index);
                        engine.handleEvent({0, 0x80, note, 0});
                    }
                }
                const auto start = std::chrono::steady_clock::now();
                engine.process(left.data(), right.data(), blockSize);
                const auto stop = std::chrono::steady_clock::now();
                durations_ms[block] = std::chrono::duration<double, std::milli>(stop - start).count();
            }
            std::sort(durations_ms.begin(), durations_ms.begin() + blocks);
            const auto percentile = [&](double value) {
                const auto index = static_cast<std::size_t>(std::ceil(value * blocks)) - 1;
                return durations_ms[std::min<std::size_t>(index, blocks - 1)];
            };
            const double callbackBudget_ms = 1000.0 * blockSize / sampleRate;
            const double totalProcess_ms = std::accumulate(
                durations_ms.begin(), durations_ms.begin() + blocks, 0.0);
            std::cout << "sample_rate_hz=" << sampleRate << " block_size=" << blockSize
                      << " polyphony=" << noteCount << " blocks=" << blocks
                      << " audio_seconds=" << audioSeconds
                      << " cpu_percent=" << 100.0 * totalProcess_ms / (audioSeconds * 1000.0)
                      << " callback_budget_ms=" << callbackBudget_ms
                      << " p50_ms=" << percentile(0.50) << " p95_ms=" << percentile(0.95)
                      << " p99_ms=" << percentile(0.99)
                      << " max_ms=" << durations_ms[blocks - 1]
                      << " peak_active_voices=" << engine.diagnostics().peakActiveVoiceCount
                      << "\n";
        }
    }
}
