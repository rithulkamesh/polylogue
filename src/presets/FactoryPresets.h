#pragma once

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

// Defaults with the preset's overrides applied.
dsp::ParamValues resolve(const FactoryPreset& preset);
// Overrides applied to an existing set, snapping each to a legal value.
void apply(const FactoryPreset& preset, dsp::ParamValues& values);

}  // namespace polylogue::presets
