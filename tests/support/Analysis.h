#pragma once

#include "dsp/Fft.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <optional>
#include <span>
#include <vector>

namespace polylogue::test {

inline bool allFinite(std::span<const float> signal)
{
    return std::all_of(signal.begin(), signal.end(), [](float x) { return std::isfinite(x); });
}

inline float peak(std::span<const float> signal)
{
    float result = 0.0f;
    for (float x : signal)
        result = std::max(result, std::abs(x));
    return result;
}

inline double mean(std::span<const float> signal)
{
    return std::accumulate(signal.begin(), signal.end(), 0.0) / static_cast<double>(signal.size());
}

inline double rms(std::span<const float> signal)
{
    double sum = 0.0;
    for (float x : signal)
        sum += static_cast<double>(x) * static_cast<double>(x);
    return std::sqrt(sum / static_cast<double>(signal.size()));
}

// Frequency from upward zero crossings with linear interpolation. Only meaningful for signals that
// cross zero once per cycle. Accurate to a small fraction of a cent over a second of audio.
inline std::optional<double> zeroCrossingFrequency(std::span<const float> signal, double sampleRate)
{
    std::vector<double> crossings;
    for (std::size_t i = 1; i < signal.size(); ++i) {
        const double a = static_cast<double>(signal[i - 1]);
        const double b = static_cast<double>(signal[i]);
        if (a < 0.0 && b >= 0.0)
            crossings.push_back(static_cast<double>(i - 1) + (-a) / (b - a));
    }
    if (crossings.size() < 3)
        return std::nullopt;
    const double cycles = static_cast<double>(crossings.size() - 1);
    return cycles * sampleRate / (crossings.back() - crossings.front());
}

inline double centsBetween(double measured, double expected)
{
    return 1200.0 * std::log2(measured / expected);
}

inline double decibels(double ratio)
{
    return 20.0 * std::log10(ratio);
}

// Energy fraction, in dB, outside +/- `guardBins` of every harmonic of `fundamentalBin`.
// Aliased components land between the harmonics, so this measures aliasing.
inline double nonHarmonicEnergyDb(std::span<const float> signal, int fftSize, int fundamentalBin,
                                  int guardBins)
{
    polylogue::dsp::Fft fft(fftSize);
    std::vector<float> spectrum(static_cast<std::size_t>(fftSize / 2));
    fft.magnitudes(signal, spectrum);

    double total = 0.0;
    double inharmonic = 0.0;
    for (int bin = 1; bin < fftSize / 2; ++bin) {
        const double power = static_cast<double>(spectrum[static_cast<std::size_t>(bin)]) *
                             static_cast<double>(spectrum[static_cast<std::size_t>(bin)]);
        total += power;
        const int nearest = (bin + fundamentalBin / 2) / fundamentalBin * fundamentalBin;
        if (std::abs(bin - nearest) > guardBins)
            inharmonic += power;
    }
    return 10.0 * std::log10(inharmonic / total + 1e-30);
}

}  // namespace polylogue::test
