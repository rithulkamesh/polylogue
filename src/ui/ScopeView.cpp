#include "ui/ScopeView.h"

#include "ui/Theme.h"

#include <algorithm>
#include <cmath>

namespace polylogue::ui {
namespace {

constexpr int kFrameRate = 60;
constexpr float kFloorDb = -84.0f;
constexpr float kCeilingDb = -6.0f;
constexpr float kFallDbPerFrame = 1.1f;
constexpr float kMinHz = 30.0f;
constexpr float kMaxHz = 16000.0f;
constexpr float kMaxGain = 8.0f;

float toDb(float magnitude)
{
    return 20.0f * std::log10(magnitude + 1e-9f);
}

}  // namespace

ScopeView::ScopeView(host::PolylogueProcessor& processor) : processor_(processor)
{
    levels_.fill(kFloorDb);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setOpaque(false);
    startTimerHz(kFrameRate);
}

void ScopeView::toggleMode()
{
    mode_ = mode_ == Mode::Wave ? Mode::Spectrum : Mode::Wave;
    repaint();
}

void ScopeView::showSpectrum(bool spectrum)
{
    mode_ = spectrum ? Mode::Spectrum : Mode::Wave;
    repaint();
}

void ScopeView::mouseDown(const juce::MouseEvent&)
{
    toggleMode();
}

void ScopeView::timerCallback()
{
    const std::size_t count = processor_.scope().writeCount();
    if (count == lastCount_)
        return;
    lastCount_ = count;

    processor_.scope().readLatest(samples_.data(), samples_.size());
    if (mode_ == Mode::Wave)
        updateWave();
    else
        updateSpectrum();
    repaint();
}

// Starts the trace on a rising zero crossing so a steady tone holds still.
void ScopeView::updateWave()
{
    int start = 0;
    for (int i = 1; i < kWindow - kShown; ++i) {
        if (samples_[static_cast<std::size_t>(i - 1)] < 0.0f &&
            samples_[static_cast<std::size_t>(i)] >= 0.0f) {
            start = i;
            break;
        }
    }

    float peak = 0.0f;
    for (int i = 0; i < kShown; ++i) {
        const float value = samples_[static_cast<std::size_t>(start + i)];
        wave_[static_cast<std::size_t>(i)] = value;
        peak = std::max(peak, std::abs(value));
    }

    const float wanted = peak > 1e-4f ? std::clamp(0.85f / peak, 1.0f, kMaxGain) : 1.0f;
    gain_ += (wanted - gain_) * 0.12f;
}

void ScopeView::updateSpectrum()
{
    fft_.magnitudes(samples_, magnitudes_);
    for (std::size_t i = 0; i < magnitudes_.size(); ++i) {
        const float level = toDb(magnitudes_[i]);
        levels_[i] = std::max(level, levels_[i] - kFallDbPerFrame);
    }
}

void ScopeView::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    if (mode_ == Mode::Wave)
        paintWave(g, area);
    else
        paintSpectrum(g, area);
}

void ScopeView::paintWave(juce::Graphics& g, juce::Rectangle<float> area) const
{
    const float middle = area.getCentreY();
    g.setColour(kBorder);
    g.drawHorizontalLine(juce::roundToInt(middle), area.getX(), area.getRight());
    g.setColour(kBorder.withAlpha(0.5f));
    for (float fraction : {0.25f, 0.75f})
        g.drawHorizontalLine(juce::roundToInt(area.getY() + area.getHeight() * fraction),
                             area.getX(), area.getRight());

    juce::Path path;
    const float halfHeight = area.getHeight() * 0.46f;
    for (int i = 0; i < kShown; ++i) {
        const float x = area.getX() + area.getWidth() * static_cast<float>(i) / (kShown - 1);
        const float y = middle - wave_[static_cast<std::size_t>(i)] * gain_ * halfHeight;
        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }
    g.setColour(kInk.withAlpha(0.92f));
    g.strokePath(path, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

void ScopeView::paintSpectrum(juce::Graphics& g, juce::Rectangle<float> area) const
{
    const float sampleRate = static_cast<float>(processor_.sampleRate());
    const float binHz = sampleRate / static_cast<float>(kWindow);

    for (float hz : {100.0f, 1000.0f, 10000.0f}) {
        const float x =
            area.getX() + area.getWidth() * std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz);
        g.setColour(kBorder);
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
    }

    juce::Path line;
    const int columns = juce::jmax(2, juce::roundToInt(area.getWidth()));
    for (int column = 0; column < columns; ++column) {
        const float t = static_cast<float>(column) / static_cast<float>(columns - 1);
        const float hz = kMinHz * std::pow(kMaxHz / kMinHz, t);
        const float position = hz / binHz;
        const auto lower =
            static_cast<std::size_t>(std::min(position, static_cast<float>(kWindow / 2 - 2)));
        const float fraction = position - static_cast<float>(lower);
        const float level = levels_[lower] * (1.0f - fraction) + levels_[lower + 1] * fraction;

        const float height = juce::jlimit(0.0f, 1.0f, (level - kFloorDb) / (kCeilingDb - kFloorDb));
        const float x = area.getX() + area.getWidth() * t;
        const float y = area.getBottom() - height * area.getHeight() * 0.94f;
        if (column == 0)
            line.startNewSubPath(x, y);
        else
            line.lineTo(x, y);
    }

    juce::Path fill(line);
    fill.lineTo(area.getRight(), area.getBottom());
    fill.lineTo(area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setColour(kInk.withAlpha(0.08f));
    g.fillPath(fill);
    g.setColour(kInk.withAlpha(0.92f));
    g.strokePath(line, juce::PathStrokeType(1.3f));

    g.setFont(mono(9.0f));
    g.setColour(kDim);
    for (float hz : {100.0f, 1000.0f, 10000.0f}) {
        const float x =
            area.getX() + area.getWidth() * std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz);
        g.drawText(hz >= 1000.0f ? juce::String(static_cast<int>(hz / 1000.0f)) + "k"
                                 : juce::String(static_cast<int>(hz)),
                   juce::Rectangle<float>(x + 3.0f, area.getY() + 2.0f, 30.0f, 11.0f),
                   juce::Justification::centredLeft);
    }
}

}  // namespace polylogue::ui
