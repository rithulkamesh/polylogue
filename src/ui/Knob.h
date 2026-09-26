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

private:
    class Dial;

    std::unique_ptr<Dial> dial_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

}  // namespace polylogue::ui
