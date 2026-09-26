#pragma once

#include <complex>
#include <span>
#include <vector>

namespace polylogue::dsp {

// Radix-2 FFT producing Hann-windowed magnitude spectra. Allocates on construction only.
class Fft {
public:
    // `size` must be a power of two.
    explicit Fft(int size);

    int size() const { return size_; }

    // Writes size/2 linear amplitudes (a full-scale sine reads about 1.0).
    void magnitudes(std::span<const float> input, std::span<float> output);

private:
    void transform();

    int size_;
    std::vector<float> window_;
    std::vector<std::complex<float>> twiddles_;
    std::vector<int> bitReversed_;
    std::vector<std::complex<float>> work_;
};

}  // namespace polylogue::dsp
