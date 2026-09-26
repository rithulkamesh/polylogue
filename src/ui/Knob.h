#pragma once

#include "ui/MappableControl.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace polylogue::ui {

// A rotary control bound to a parameter.
class Knob final : public MappableControl {
public:
    Knob(host::PolylogueProcessor& processor, dsp::Param param, juce::String label);
    ~Knob() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Clicking a knob without dragging opens a text box over it for typing a value, such as
    // "1.5k", "250 ms" or "-6". Enter applies it and Escape cancels.
    void beginTextEntry();
    bool isEditingText() const;
    // Sets the parameter from typed text. Returns false if the text cannot be read.
    bool applyTypedValue(const juce::String& text);

private:
    class Dial;

    void endTextEntry();

    std::unique_ptr<Dial> dial_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
    juce::TextEditor entry_;
};

}  // namespace polylogue::ui
