#pragma once

#include "dsp/Parameters.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace polylogue::dsp {

// A second, smaller way to describe a sound: eight perceptual axes instead of thirty-nine
// parameters. Each axis is a position from 0 to 1, and moving it passes through different kinds
// of sound rather than rescaling one parameter.
enum class Axis : std::uint8_t {
    Wave,     // triangle -> saw -> square
    Metal,    // thick and harmonic -> sync and ring growl -> FM bell
    Grit,     // clean -> driven -> resonant and noisy
    Bright,   // dark -> open
    Attack,   // hard click -> slow swell
    Sustain,  // short hit -> long ring -> held
    Evolve,   // opens over time <- static (0.5) -> closes over time
    Motion,   // still -> vibrato -> wobble -> wide chorus
    Count
};

inline constexpr std::size_t kAxisCount = static_cast<std::size_t>(Axis::Count);

struct Axes {
    std::array<float, kAxisCount> values{};

    float& operator[](Axis axis) { return values[static_cast<std::size_t>(axis)]; }
    const float& operator[](Axis axis) const { return values[static_cast<std::size_t>(axis)]; }

    // A plain, static, unmodulated saw.
    static Axes neutral();
};

const char* axisName(Axis axis);

// The play knobs and their home positions as stored in a parameter set.
Axes knobsOf(const ParamValues& stored);
Axes homeOf(const ParamValues& stored);
void setKnobs(ParamValues& stored, const Axes& axes);
void setHome(ParamValues& stored, const Axes& axes);

// The sound the engine plays: the stored sound moved by however far each knob is from its home.
// With every knob at home this is the stored sound exactly, so a preset sounds as it was made and
// the knobs then move it from there.
ParamValues effectiveValues(const ParamValues& stored);

// Commits the knob offsets into the stored sound and sets home to the knobs, so the panel shows
// the sound as it is playing and can be fine-tuned from there.
ParamValues bake(const ParamValues& stored);

// The full parameter set for a set of axis positions. `base` supplies everything the axes do not
// describe: level, voice count, key mode, octave, tune, bend range and glide.
ParamValues fromAxes(const Axes& axes, const ParamValues& base = ParamValues::defaults());

}  // namespace polylogue::dsp
