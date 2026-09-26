#pragma once

#include "dsp/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace polylogue::host {

// Factory and user presets, and the notion of "the sound currently loaded". Message thread only.
//
// A preset is every parameter's value, so loading one is deterministic. The MIDI map is
// deliberately not part of a preset: changing sounds must not unmap the user's controller.
class PresetManager {
public:
    struct Entry {
        juce::String name;
        juce::String category;
        int factoryIndex = -1;
        juce::File file;

        bool isFactory() const { return factoryIndex >= 0; }
    };

    // User presets live in `dataDirectory / "Presets"`.
    PresetManager(juce::AudioProcessorValueTreeState& parameters, const juce::File& dataDirectory);

    static juce::File defaultDataDirectory();

    // Factory presets first, in the order they were authored, then user presets by name.
    const std::vector<Entry>& entries() const { return entries_; }
    void rescanUserPresets();

    void load(const Entry& entry);
    void loadFactory(int index);
    // Writes the current sound to the user directory under `name`.
    bool saveUser(const juce::String& name);
    bool deleteUser(const Entry& entry);

    dsp::ParamValues currentValues() const;
    // Sets every parameter, notifying the host. Forgets any edits-since-load.
    void setValues(const dsp::ParamValues& values);
    // Restores a saved session, including whether it had been edited since its preset was loaded.
    void restore(const juce::String& name, const dsp::ParamValues& values, bool modified);

    const juce::String& currentName() const { return currentName_; }
    bool isModified() const;
    // Index into the factory list, or -1 for a user preset or an unnamed sound.
    int currentFactoryIndex() const;

private:
    void applyValues(const dsp::ParamValues& values);
    void markLoaded(const juce::String& name, bool modified);

    juce::AudioProcessorValueTreeState& parameters_;
    juce::File presetDirectory_;
    std::vector<Entry> entries_;
    juce::String currentName_ = "Init";
    dsp::ParamValues loaded_ = dsp::ParamValues::defaults();
    bool forceModified_ = false;
};

}  // namespace polylogue::host
