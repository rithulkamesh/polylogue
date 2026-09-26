#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace polylogue::ui {

class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                          float startAngle, float endAngle, juce::Slider& slider) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour, bool highlighted,
                              bool down) override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
    void drawCornerResizer(juce::Graphics& g, int width, int height, bool mouseOver,
                           bool dragging) override;
};

}  // namespace polylogue::ui
