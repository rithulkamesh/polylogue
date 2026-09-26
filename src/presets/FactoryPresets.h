#pragma once

#include "dsp/Axes.h"
#include "dsp/Parameters.h"

#include <span>

namespace polylogue::presets {

struct PresetValue {
    dsp::Param param;
    float value;  // plain units
};

// A preset lists only what differs from the parameter defaults.
struct FactoryPreset {
    const char* name;
    const char* category;
    std::span<const PresetValue> values;
};

std::span<const FactoryPreset> factoryPresets();
std::span<const char* const> presetCategories();

// Where the eight play knobs sit for this sound: the position whose sound is closest to it, found
// by tools/fit. The knobs move the sound from here.
dsp::Axes homePosition(const FactoryPreset& preset);

// Defaults with the preset's overrides applied, and the play knobs at the preset's home.
dsp::ParamValues resolve(const FactoryPreset& preset);
// Overrides applied to an existing set, snapping each to a legal value.
void apply(const FactoryPreset& preset, dsp::ParamValues& values);

}  // namespace polylogue::presets
