#pragma once

#include "host/PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>

namespace polylogue::ui {

// The on-screen keyboard. Keys played here go to the synth through the processor's lock-free queue,
// and notes arriving from a hardware keyboard light the keys here.
class KeyboardBar final : public juce::Component, private juce::MidiKeyboardStateListener {
public:
    explicit KeyboardBar(host::PolylogueProcessor& processor);
    ~KeyboardBar() override;

    // Moves the lit keys to match MIDI that has arrived since the last call.
    void followMidi();

    void resized() override;

private:
    class Keys;

    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;

    host::PolylogueProcessor& processor_;
    juce::MidiKeyboardState state_;
    std::unique_ptr<Keys> keys_;
    bool followingMidi_ = false;
};

}  // namespace polylogue::ui
