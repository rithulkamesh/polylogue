#pragma once

#include "dsp/Settings.h"
#include "dsp/Smoother.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace polylogue::dsp {

enum class ModSource : std::size_t {
    ModEnv,
    Lfo,
    Count
};

enum class ModDestination : std::size_t {
    Osc1Pitch,  // semitones
    Osc2Pitch,  // semitones
    Osc1Shape,  // shape units, 0..1 across the knob
    Osc2Shape,
    Cutoff,     // octaves
    Osc1Level,  // level units, 0..1 across the knob
    Osc2Level,
    Count
};

inline constexpr std::size_t kModSourceCount = static_cast<std::size_t>(ModSource::Count);
inline constexpr std::size_t kModDestinationCount = static_cast<std::size_t>(ModDestination::Count);

using ModSourceValues = std::array<float, kModSourceCount>;
using ModOffsets = std::array<float, kModDestinationCount>;

// Sources times per-route amounts, summed per destination. The panel's target switches decide
// which amounts are non-zero. Every amount is smoothed, so moving a knob or flipping a target
// crossfades instead of clicking.
//
// To add a source or destination: extend the enum, then give it a row in `configure`.
class ModMatrix {
public:
    void prepare(double sampleRate);
    void configure(const SynthSettings& settings);
    void snap();

    ModOffsets evaluate(const ModSourceValues& sources);

private:
    void set(ModSource source, ModDestination destination, float amount);

    std::array<std::array<Smoother, kModDestinationCount>, kModSourceCount> amounts_;
};

constexpr std::size_t index(ModSource source)
{
    return static_cast<std::size_t>(source);
}

constexpr std::size_t index(ModDestination destination)
{
    return static_cast<std::size_t>(destination);
}

}  // namespace polylogue::dsp
