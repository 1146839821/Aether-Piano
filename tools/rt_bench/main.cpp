#include "dsp/PianoEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>

int main() {
    constexpr std::uint32_t sampleRate = 48000;
    constexpr std::uint32_t benchmarkSeconds = 10;
    constexpr std::uint64_t totalSamples = std::uint64_t(sampleRate) * benchmarkSeconds;
    constexpr std::array<std::uint32_t, 4> blockSizes{64, 128, 256, 512};
    constexpr std::size_t maxBlocks = totalSamples / blockSizes.front() + 1;
    std::array<double, maxBlocks> durationsMs{};
    std::array<float, 512> left{}, right{};

    for (const auto blockSize : blockSizes) {
        aether::PianoEngine engine;
        engine.prepare(sampleRate, blockSize);
        if (!engine.isPrepared()) {
            std::cerr << "failed to prepare benchmark engine\n";
            return 1;
        }
        engine.handleEvent({0, 0x90, 60, 80});
        const std::uint64_t blocks = (totalSamples + blockSize - 1) / blockSize;
        std::uint64_t done = 0;
        for (std::uint64_t index = 0; index < blocks; ++index) {
            if (done >= sampleRate * 5 && done < sampleRate * 5 + blockSize)
                engine.handleEvent({0, 0x80, 60, 0});
            const auto count = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(blockSize, totalSamples - done));
            const auto start = std::chrono::steady_clock::now();
            engine.process(left.data(), right.data(), count);
            const auto stop = std::chrono::steady_clock::now();
            durationsMs[index] = std::chrono::duration<double, std::milli>(stop - start).count();
            done += count;
        }
        std::sort(durationsMs.begin(), durationsMs.begin() + blocks);
        const auto percentile = [&](double p) {
            const auto index = static_cast<std::size_t>(std::ceil(p * blocks)) - 1;
            return durationsMs[std::min<std::size_t>(index, blocks - 1)];
        };
        const double realtimeBlockMs = 1000.0 * blockSize / sampleRate;
        const double totalProcessMs = std::accumulate(
            durationsMs.begin(), durationsMs.begin() + blocks, 0.0);
        std::cout << "sample_rate_hz=" << sampleRate << " block_size=" << blockSize
                  << " blocks=" << blocks << " audio_seconds=" << benchmarkSeconds
                  << " cpu_percent=" << 100.0 * totalProcessMs / (benchmarkSeconds * 1000.0)
                  << " callback_budget_ms=" << realtimeBlockMs
                  << " p50_ms=" << percentile(0.50) << " p95_ms=" << percentile(0.95)
                  << " p99_ms=" << percentile(0.99)
                  << " max_ms=" << durationsMs[blocks - 1] << '\n';
    }
}
