#pragma once

#include "host/PluginProcessor.h"
#include "ui/KeyboardBar.h"
#include "ui/Lcd.h"
#include "ui/LookAndFeel.h"
#include "ui/MappableControl.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace polylogue::ui {

// The whole interface: the display, the panel of controls, the keyboard, and two small buttons.
class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PluginEditor(host::PolylogueProcessor& processor);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    Lcd& display() { return lcd_; }

private:
    static constexpr int kBaseWidth = 1200;
    static constexpr int kBaseHeight = 636;

    struct SectionMark {
        juce::String title;
        juce::Rectangle<float> area;  // in base units
    };

    void timerCallback() override;
    void pollParameters();
    void pollMidi();
    void setMapMode(bool on);
    void describe(dsp::Param param);

    host::PolylogueProcessor& processor_;
    juce::SharedResourcePointer<LookAndFeel> lookAndFeel_;
    Lcd lcd_;
    KeyboardBar keyboard_;
    std::vector<std::unique_ptr<MappableControl>> controls_;
    std::vector<SectionMark> marks_;
    juce::TextButton saveButton_{"SAVE"};
    juce::TextButton mapButton_{"MAP"};

    std::array<float, dsp::kParamCount> lastPositions_{};
    bool firstPoll_ = true;
    std::uint32_t lastMidiActivity_ = 0;
    std::uint32_t lastMapperActivity_ = 0;
    std::uint32_t lastMapVersion_ = 0;
    float ledLevel_ = 0.0f;
    bool mapMode_ = false;
    juce::Component::SafePointer<juce::AlertWindow> dialog_;
};

}  // namespace polylogue::ui
