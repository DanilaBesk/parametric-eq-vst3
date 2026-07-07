#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace eq
{
template <std::size_t Capacity, std::size_t BlockSize>
class RecentSampleFifo
{
public:
    static_assert(Capacity > BlockSize, "Capacity must be larger than BlockSize");

    void reset()
    {
        for (auto& sample : samples)
            sample.store(0.0f, std::memory_order_relaxed);

        readCounter.store(0, std::memory_order_relaxed);
        writeCounter.store(0, std::memory_order_relaxed);
    }

    void push(float sample)
    {
        const auto write = writeCounter.load(std::memory_order_relaxed);
        samples[write % Capacity].store(sample, std::memory_order_relaxed);

        const auto nextWrite = write + 1;
        writeCounter.store(nextWrite, std::memory_order_release);

        const auto oldestReadable = nextWrite > Capacity ? nextWrite - Capacity : 0;
        auto read = readCounter.load(std::memory_order_acquire);

        while (read < oldestReadable
               && !readCounter.compare_exchange_weak(read, oldestReadable, std::memory_order_release, std::memory_order_acquire))
        {
        }
    }

    bool pull(std::array<float, BlockSize>& target)
    {
        auto read = readCounter.load(std::memory_order_relaxed);
        const auto write = writeCounter.load(std::memory_order_acquire);

        if (write - read < BlockSize)
            return false;

        const auto newestBlockStart = write - BlockSize;
        if (read < newestBlockStart)
            read = newestBlockStart;

        for (std::size_t index = 0; index < BlockSize; ++index)
            target[index] = samples[(read + index) % Capacity].load(std::memory_order_relaxed);

        readCounter.store(read + BlockSize, std::memory_order_release);
        return true;
    }

private:
    std::array<std::atomic<float>, Capacity> samples {};
    std::atomic<std::size_t> readCounter { 0 };
    std::atomic<std::size_t> writeCounter { 0 };
};
}
