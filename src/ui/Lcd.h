#pragma once

#include "host/PluginProcessor.h"
#include "ui/ScopeView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>

namespace polylogue::ui {

// The display: preset name and browser on top, the visualizer in the middle, and a status line
// that shows the last knob touched, MIDI learn prompts, or what the synth is doing.
class Lcd final : public juce::Component, private juce::Timer {
public:
    explicit Lcd(host::PolylogueProcessor& processor);

    // A knob value for a moment, e.g. CUTOFF 1.20 kHz.
    void showReadout(const juce::String& label, const juce::String& value,
                     const juce::String& note);
    ScopeView& visualizer() { return scope_; }

    // A short status word such as SAVED.
    void showMessage(const juce::String& text);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

private:
    struct Layout {
        juce::Rectangle<float> nameRow;
        juce::Rectangle<float> nameArea;
        juce::Rectangle<float> previous;
        juce::Rectangle<float> next;
        juce::Rectangle<float> infoRow;
        juce::Rectangle<float> statusRow;
        juce::Rectangle<float> scope;
    };

    Layout layout() const;
    void timerCallback() override;
    void paintNameRow(juce::Graphics& g, const Layout& area) const;
    void paintStatusRow(juce::Graphics& g, const Layout& area) const;

    host::PolylogueProcessor& processor_;
    ScopeView scope_;

    juce::String readoutLabel_;
    juce::String readoutValue_;
    juce::String readoutNote_;
    juce::String message_;
    std::int64_t readoutUntil_ = 0;
    std::int64_t messageUntil_ = 0;

    // What the last paint showed, so the timer repaints only on change.
    juce::String shownName_;
    bool shownModified_ = false;
    bool shownLearning_ = false;
    int shownController_ = -2;
    bool shownReadout_ = false;
    bool shownMessage_ = false;
};

}  // namespace polylogue::ui
