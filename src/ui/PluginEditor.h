#pragma once

#include "host/PluginProcessor.h"
#include "ui/KeyboardBar.h"
#include "ui/Knob.h"
#include "ui/Lcd.h"
#include "ui/LookAndFeel.h"
#include "ui/MappableControl.h"
#include "ui/ScopeView.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace polylogue::ui {

// The whole interface: the display, the panel of controls, the keyboard, and two small buttons.
class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PluginEditor(host::PolylogueProcessor& owner);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    Lcd& display() { return lcd_; }

    // PLAY is eight large macro knobs; EDIT is the full panel. Right-click a control on EDIT and
    // assign it to a macro, and that macro knob moves that one parameter and nothing else.
    enum class Screen {
        Play,
        Edit
    };
    // With `animate` the two screens crossfade; without, the switch is immediate.
    void showScreen(Screen screen, bool animate = true);
    Screen screen() const { return screen_; }
    dsp::Param macroKnobTarget(std::size_t macro) const { return playKnobs_[macro]->param(); }

private:
    static constexpr int kBaseWidth = 1200;
    static constexpr int kBaseHeight = 636;

    struct SectionMark {
        juce::String title;
        juce::Rectangle<float> area;  // in base units
        bool onPlayScreen = false;
    };

    void timerCallback() override;
    void pollParameters();
    void pollMidi();
    void setMapMode(bool on);
    std::unique_ptr<Knob> makeMacroKnob(std::size_t macro);
    void refreshMacros();
    void applyScreenMix();
    void stepTransition();
    void layoutPlayScreen(float scale);
    template<typename Visitor>
    void forEachControl(Visitor&& visitor);
    void describe(dsp::Param param);

    host::PolylogueProcessor& processor_;
    juce::SharedResourcePointer<LookAndFeel> lookAndFeel_;
    Lcd lcd_;
    KeyboardBar keyboard_;
    std::vector<std::unique_ptr<MappableControl>> controls_;
    std::vector<std::unique_ptr<Knob>> playKnobs_;
    std::array<dsp::Param, dsp::kMacroCount> macroTargets_{};  // what each play knob is showing
    std::vector<SectionMark> marks_;
    juce::Rectangle<float> stagePanel_;  // in base units
    juce::TextButton saveButton_{"SAVE"};
    juce::TextButton mapButton_{"MAP"};
    juce::TextButton playButton_{"PLAY"};
    juce::TextButton editButton_{"EDIT"};
    Screen screen_ = Screen::Play;

    // 0 is the edit panel, 1 the play screen; between them the two are crossfading.
    struct Fader final : juce::Timer {
        std::function<void()> tick;
        void timerCallback() override { tick(); }
    };
    Fader fader_;
    ScopeView stage_;
    float progress_ = 1.0f;
    float mix_ = 1.0f;

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
