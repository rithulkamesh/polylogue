#pragma once

#include "ui/MappableControl.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace polylogue::ui {

// A switch: one chip per position, the selected one filled. Used for choices and short integer
// ranges such as the octave.
class ChipGroup final : public MappableControl {
public:
    enum class Layout {
        Column,
        Row
    };

    ChipGroup(host::PolylogueProcessor& processor, dsp::Param param, juce::String label,
              Layout layout);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;

    // How many positions the parameter has.
    static int positions(const dsp::ParamSpec& spec);

private:
    juce::String chipText(int position) const;
    juce::Rectangle<float> chipBounds(int position) const;
    int positionAt(juce::Point<float> point) const;

    Layout layout_;
    int selected_ = 0;
    juce::ParameterAttachment attachment_;
};

}  // namespace polylogue::ui
