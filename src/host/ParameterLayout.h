#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace polylogue::host {

// Increment only when a parameter's meaning changes in a way old sessions must not see.
inline constexpr int kParameterVersion = 1;

// Builds the host-visible parameters from the table in dsp/Parameters.h, so the DSP and the host
// cannot disagree about ranges or mappings.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

}  // namespace polylogue::host
