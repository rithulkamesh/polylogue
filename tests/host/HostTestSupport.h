#pragma once

#include "host/PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace polylogue::test {

// JUCE's message manager is created for the whole run by the listener in JuceListener.cpp.
inline void ensureJuceInitialised() {}

// A scratch directory that disappears with the test, so nothing touches the real user data.
struct TempDirectory {
    juce::File directory;

    TempDirectory()
        : directory(juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("polylogue-tests-" + juce::Uuid().toString()))
    {
        directory.createDirectory();
    }
    ~TempDirectory() { directory.deleteRecursively(); }
};

inline host::PolylogueProcessor::Options optionsFor(const juce::File& directory)
{
    host::PolylogueProcessor::Options options;
    options.dataDirectory = directory;
    return options;
}

// A processor prepared for 48 kHz with private storage.
struct Rig {
    static constexpr double kSampleRate = 48000.0;

    TempDirectory storage;
    host::PolylogueProcessor processor;

    Rig() : Rig(nullptr) {}

    void set(dsp::Param param, float plain)
    {
        auto* p = processor.parameter(param);
        p->setValueNotifyingHost(p->convertTo0to1(plain));
    }

    float get(dsp::Param param) const
    {
        return processor.parameters().getRawParameterValue(dsp::paramSpec(param).id)->load();
    }

    // Renders one block and returns the left channel.
    std::vector<float> run(juce::MidiBuffer& midi, int samples = 512)
    {
        juce::AudioBuffer<float> buffer(2, samples);
        buffer.clear();
        processor.processBlock(buffer, midi);
        return {buffer.getReadPointer(0), buffer.getReadPointer(0) + samples};
    }

    std::vector<float> run(int samples = 512)
    {
        juce::MidiBuffer none;
        return run(none, samples);
    }

private:
    explicit Rig(std::nullptr_t) : processor(ensureThenOptions())
    {
        processor.prepareToPlay(kSampleRate, 512);
    }

    host::PolylogueProcessor::Options ensureThenOptions()
    {
        ensureJuceInitialised();
        return optionsFor(storage.directory);
    }
};

}  // namespace polylogue::test
