#pragma once

#include "dsp/Engine.h"
#include "dsp/Parameters.h"
#include "host/MidiMapper.h"
#include "host/MidiTranslator.h"
#include "host/PresetManager.h"
#include "host/ScopeBuffer.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <span>

namespace polylogue::host {

class PolylogueProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    using EditorFactory = std::function<juce::AudioProcessorEditor*(PolylogueProcessor&)>;

    struct Options {
        EditorFactory editor;
        // Where user presets and the MIDI map are kept.
        juce::File dataDirectory = PresetManager::defaultDataDirectory();
    };

    PolylogueProcessor();
    explicit PolylogueProcessor(Options options);
    ~PolylogueProcessor() override;

    juce::AudioProcessorValueTreeState& parameters() { return parameters_; }
    const juce::AudioProcessorValueTreeState& parameters() const { return parameters_; }
    juce::RangedAudioParameter* parameter(dsp::Param param) const;
    MidiMapper& midiMapper() { return mapper_; }
    PresetManager& presets() { return presets_; }
    const ScopeBuffer& scope() const { return scope_; }
    double sampleRate() const { return sampleRateHz_.load(std::memory_order_relaxed); }
    // Increments for every block that carried MIDI, for an activity light.
    std::uint32_t midiActivity() const { return midiActivity_.load(std::memory_order_relaxed); }

    // A note from the on-screen keyboard, played ahead of the host's MIDI. Velocity 0 releases it.
    // Message thread only.
    void queueKeyboardNote(int note, float velocity);

    struct KeyEvent {
        int note = 0;
        float velocity = 0.0f;  // 0 for a release
    };
    // The next note that arrived from MIDI, so the on-screen keys can follow a hardware keyboard.
    // Message thread only.
    bool popMonitoredNote(KeyEvent& event);

    // The parameter each PLAY macro knob moves. Message thread only.
    dsp::Param macroTarget(std::size_t macro) const;
    void assignMacro(std::size_t macro, dsp::Param target);

    // Delivers controller-driven control moves to the parameters. Called by the timer; exposed so a
    // caller without a running message loop can drive it.
    void processPendingControlChanges();

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void prepareToPlay(double sampleRate, int maxBlockSize) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;

    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& name) override;

    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    static constexpr std::size_t kMaxEventsPerBlock = 2048;
    static constexpr std::size_t kMaxControlsPerBlock = 256;
    static constexpr std::size_t kControlFifoSize = 128;
    static constexpr std::size_t kKeyFifoSize = 256;
    static constexpr std::size_t kMaxKeyboardEventsPerBlock = 64;
    static constexpr double kOverlaySeconds = 0.5;

    // A control moved by a controller takes effect in the sound at once; the parameter itself
    // catches up on the message thread. The overlay covers the gap.
    struct Overlay {
        float plain = 0.0f;
        float normalized = 0.0f;
        long samplesLeft = 0;
    };

    struct ControlMessage {
        int param = 0;
        float normalized = 0.0f;
    };

    void timerCallback() override;
    std::size_t drainKeyboardQueue();
    void monitorNotes(std::span<const dsp::MidiEvent> events);
    void applyControlChanges(std::span<const ControlChange> controls);
    dsp::ParamValues readParameters(int blockSamples);
    juce::File midiMapFile() const;
    void writeMidiMap() const;
    bool readMidiMap(const juce::XmlElement& xml);

    juce::AudioProcessorValueTreeState parameters_;
    EditorFactory editorFactory_;
    juce::File dataDirectory_;
    PresetManager presets_;
    MidiMapper mapper_;
    ScopeBuffer scope_;
    dsp::Engine engine_;

    std::array<std::atomic<float>*, dsp::kParamCount> rawValues_{};
    std::array<dsp::MidiEvent, kMaxEventsPerBlock> events_{};
    std::array<ControlChange, kMaxControlsPerBlock> controls_{};
    std::array<Overlay, dsp::kParamCount> overlays_{};
    double sampleRate_ = 44100.0;
    std::atomic<double> sampleRateHz_{44100.0};

    juce::AbstractFifo controlFifo_{static_cast<int>(kControlFifoSize)};
    std::array<ControlMessage, kControlFifoSize> controlQueue_{};

    // On-screen keyboard to audio thread, and MIDI notes back to the display.
    juce::AbstractFifo keyboardFifo_{static_cast<int>(kKeyFifoSize)};
    std::array<KeyEvent, kKeyFifoSize> keyboardQueue_{};
    juce::AbstractFifo monitorFifo_{static_cast<int>(kKeyFifoSize)};
    std::array<KeyEvent, kKeyFifoSize> monitorQueue_{};

    std::atomic<std::uint32_t> midiActivity_{0};
    std::uint32_t savedMapVersion_ = 0;
};

}  // namespace polylogue::host
