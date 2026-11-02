#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <numeric>
#include <tuple>
#include <vector>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace {
std::atomic<bool> countAllocations{false};
std::atomic<std::uint64_t> allocationCount{0};

double percentile(const std::vector<double>& sortedMs, double quantile) {
    const auto index = static_cast<std::size_t>(
        std::ceil(quantile * static_cast<double>(sortedMs.size()))) - 1;
    return sortedMs[std::min(index, sortedMs.size() - 1)];
}

bool runScenario(std::uint32_t blockSize, std::uint32_t voiceCount) {
    constexpr std::uint32_t sampleRate = 48000;
    constexpr std::uint32_t audioSeconds = 3;
    constexpr std::uint32_t releaseSeconds = 2;
    const auto totalSamples = sampleRate * audioSeconds;
    const auto blockCount = totalSamples / blockSize;
    const auto releaseBlock = sampleRate * releaseSeconds / blockSize;
    const double budgetMs = 1000.0 * blockSize / sampleRate;

    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    if (!processor)
        return false;
    processor->prepareToPlay(sampleRate, static_cast<int>(blockSize));

    juce::AudioBuffer<float> audio(2, static_cast<int>(blockSize));
    juce::MidiBuffer midi;
    midi.ensureSize(static_cast<std::size_t>(voiceCount) * 16u);
    std::vector<double> durationsMs;
    durationsMs.reserve(blockCount);
    std::vector<std::tuple<std::uint32_t, std::uint32_t, double>> overruns;
    std::uint64_t callbackAllocations = 0;
    std::uint64_t nonFiniteSamples = 0;
    float peakOutput = 0.0f;

    for (std::uint32_t block = 0; block < blockCount; ++block) {
        midi.clear();
        std::uint32_t eventCount = 0;
        if (block == 0) {
            for (std::uint32_t index = 0; index < voiceCount; ++index) {
                const auto note = static_cast<std::uint8_t>(21 + index);
                midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<std::uint8_t>(80)), 0);
                ++eventCount;
            }
        } else if (block == releaseBlock) {
            for (std::uint32_t index = 0; index < voiceCount; ++index) {
                const auto note = static_cast<std::uint8_t>(21 + index);
                midi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
                ++eventCount;
            }
        }
        audio.clear();
        const auto start = std::chrono::steady_clock::now();
        countAllocations.store(true, std::memory_order_relaxed);
        processor->processBlock(audio, midi);
        countAllocations.store(false, std::memory_order_relaxed);
        const auto stop = std::chrono::steady_clock::now();
        const double elapsedMs = std::chrono::duration<double, std::milli>(stop - start).count();
        durationsMs.push_back(elapsedMs);
        if (elapsedMs > budgetMs)
            overruns.emplace_back(block, eventCount, elapsedMs);
        callbackAllocations += allocationCount.exchange(0, std::memory_order_relaxed);

        for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
            const auto* data = audio.getReadPointer(channel);
            for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
                if (!std::isfinite(data[sample]))
                    ++nonFiniteSamples;
                peakOutput = std::max(peakOutput, std::abs(data[sample]));
            }
        }
    }

    std::sort(durationsMs.begin(), durationsMs.end());
    const double totalMs = std::accumulate(durationsMs.begin(), durationsMs.end(), 0.0);
    std::cout << "sample_rate_hz=" << sampleRate
              << " block_size=" << blockSize
              << " polyphony=" << voiceCount
              << " callbacks=" << blockCount
              << " audio_seconds=" << audioSeconds
              << " callback_budget_ms=" << budgetMs
              << " p50_ms=" << percentile(durationsMs, 0.50)
              << " p95_ms=" << percentile(durationsMs, 0.95)
              << " p99_ms=" << percentile(durationsMs, 0.99)
              << " max_ms=" << durationsMs.back()
              << " over_deadline_count=" << overruns.size()
              << " new_allocations=" << callbackAllocations
              << " non_finite_samples=" << nonFiniteSamples
              << " peak_output=" << peakOutput
              << " cpu_percent=" << 100.0 * totalMs / (audioSeconds * 1000.0)
              << '\n';
    for (const auto& [block, events, elapsedMs] : overruns)
        std::cout << "  overrun_block=" << block << " midi_events=" << events
                  << " elapsed_ms=" << elapsedMs << '\n';

    processor->releaseResources();
    return callbackAllocations == 0 && nonFiniteSamples == 0;
}
} // namespace

void* operator new(std::size_t size) {
    if (countAllocations.load(std::memory_order_relaxed))
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (countAllocations.load(std::memory_order_relaxed))
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    constexpr std::uint32_t blockSizes[]{64, 128};
    constexpr std::uint32_t voiceCounts[]{1, 8, 32, 64};
    for (const auto blockSize : blockSizes) {
        for (const auto voiceCount : voiceCounts) {
            if (!runScenario(blockSize, voiceCount))
                return 1;
        }
    }
    return 0;
}
