#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace polylogue::host {

// Hands the most recent audio from the audio thread to the UI without locks. One writer, any
// reader. A read that overlaps a write may see a few samples from the newer block, which is
// harmless for drawing.
class ScopeBuffer {
public:
    static constexpr std::size_t kSize = 8192;

    void write(const float* samples, int count)
    {
        std::size_t position = writeIndex_.load(std::memory_order_relaxed);
        for (int i = 0; i < count; ++i)
            data_[(position++) & (kSize - 1)].store(samples[i], std::memory_order_relaxed);
        writeIndex_.store(position, std::memory_order_release);
    }

    // Total samples written so far; changes whenever new audio has arrived.
    std::size_t writeCount() const { return writeIndex_.load(std::memory_order_acquire); }

    // Copies the newest `count` samples, oldest first. `count` must not exceed kSize.
    void readLatest(float* out, std::size_t count) const
    {
        const std::size_t end = writeIndex_.load(std::memory_order_acquire);
        for (std::size_t i = 0; i < count; ++i)
            out[i] = data_[(end - count + i) & (kSize - 1)].load(std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<float>, kSize> data_{};
    std::atomic<std::size_t> writeIndex_{0};
};

}  // namespace polylogue::host
