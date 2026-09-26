#pragma once

#include "dsp/Parameters.h"
#include "host/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace polylogue::ui {

// A control on the panel bound to one parameter, with a caption showing its label and the MIDI
// controller mapped to it. Knobs and switches share this, so every control can be MIDI-learned in
// the same way: right-click, or press MAP and click.
class MappableControl : public juce::Component {
public:
    MappableControl(host::PolylogueProcessor& processor, dsp::Param param, juce::String label);

    dsp::Param param() const { return param_; }

    // While the panel is in map mode a click arms the control for MIDI learn instead of using it.
    void setMapMode(bool on);
    void refreshMapping();

protected:
    // Height reserved at the bottom for the caption.
    int captionHeight() const;
    void paintCaption(juce::Graphics& g) const;
    void startLearning();
    void showContextMenu();

    bool mapMode() const { return mapMode_; }
    bool learning() const { return learning_; }

    host::PolylogueProcessor& processor_;

private:
    dsp::Param param_;
    juce::String label_;
    bool mapMode_ = false;
    bool learning_ = false;
    int controller_ = host::MidiMapper::kUnbound;
};

}  // namespace polylogue::ui
