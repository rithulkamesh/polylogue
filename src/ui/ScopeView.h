#pragma once

#include "dsp/Fft.h"
#include "host/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

namespace polylogue::ui {

// The visualizer: an oscilloscope with a rising-edge trigger, or a spectrum. Click to switch.
class ScopeView final : public juce::Component, private juce::Timer {
public:
    explicit ScopeView(host::PolylogueProcessor& processor);

    void toggleMode();
    void showSpectrum(bool spectrum);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    enum class Mode {
        Wave,
        Spectrum
    };

    static constexpr int kWindow = 2048;
    static constexpr int kShown = 1024;

    void timerCallback() override;
    void updateWave();
    void updateSpectrum();
    void paintWave(juce::Graphics& g, juce::Rectangle<float> area) const;
    void paintSpectrum(juce::Graphics& g, juce::Rectangle<float> area) const;

    host::PolylogueProcessor& processor_;
    Mode mode_ = Mode::Wave;
    std::size_t lastCount_ = 0;

    std::array<float, kWindow> samples_{};
    std::array<float, kShown> wave_{};
    float gain_ = 1.0f;

    dsp::Fft fft_{kWindow};
    std::array<float, kWindow / 2> magnitudes_{};
    std::array<float, kWindow / 2> levels_{};  // smoothed, in dB
};

}  // namespace polylogue::ui
