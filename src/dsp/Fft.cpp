#include "dsp/Fft.h"

#include <cassert>
#include <cmath>
#include <numbers>
#include <utility>

namespace polylogue::dsp {

Fft::Fft(int size)
    : size_(size),
      window_(static_cast<std::size_t>(size)),
      twiddles_(static_cast<std::size_t>(size / 2)),
      bitReversed_(static_cast<std::size_t>(size)),
      work_(static_cast<std::size_t>(size))
{
    assert(size >= 2 && (size & (size - 1)) == 0);

    const auto n = static_cast<std::size_t>(size);
    for (std::size_t i = 0; i < n; ++i) {
        const double phase =
            2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(n);
        window_[i] = static_cast<float>(0.5 - 0.5 * std::cos(phase));
    }
    for (std::size_t i = 0; i < n / 2; ++i) {
        const double angle =
            -2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(n);
        twiddles_[i] = {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
    }

    int bits = 0;
    while ((1 << bits) < size)
        ++bits;
    for (int i = 0; i < size; ++i) {
        int reversed = 0;
        for (int b = 0; b < bits; ++b)
            reversed |= ((i >> b) & 1) << (bits - 1 - b);
        bitReversed_[static_cast<std::size_t>(i)] = reversed;
    }
}

void Fft::magnitudes(std::span<const float> input, std::span<float> output)
{
    assert(input.size() >= static_cast<std::size_t>(size_));
    assert(output.size() >= static_cast<std::size_t>(size_ / 2));

    for (int i = 0; i < size_; ++i) {
        const auto src = static_cast<std::size_t>(bitReversed_[static_cast<std::size_t>(i)]);
        work_[static_cast<std::size_t>(i)] = input[src] * window_[src];
    }
    transform();

    // A sine of amplitude A peaks at A * N / 4 after the Hann window (coherent gain 0.5).
    const float scale = 4.0f / static_cast<float>(size_);
    for (std::size_t i = 0; i < static_cast<std::size_t>(size_ / 2); ++i)
        output[i] = std::abs(work_[i]) * scale;
}

void Fft::transform()
{
    for (int length = 2; length <= size_; length <<= 1) {
        const int half = length / 2;
        const int stride = size_ / length;
        for (int start = 0; start < size_; start += length) {
            for (int k = 0; k < half; ++k) {
                const auto a = static_cast<std::size_t>(start + k);
                const auto b = static_cast<std::size_t>(start + k + half);
                const std::complex<float> t =
                    work_[b] * twiddles_[static_cast<std::size_t>(k * stride)];
                work_[b] = work_[a] - t;
                work_[a] += t;
            }
        }
    }
}

}  // namespace polylogue::dsp
