#include "ui/Theme.h"

#include "BinaryData.h"

namespace polylogue::ui {
namespace {

juce::Typeface::Ptr load(const char* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor(data, static_cast<std::size_t>(size));
}

juce::Typeface::Ptr plexSans()
{
    static const auto typeface =
        load(BinaryData::IBMPlexSansRegular_ttf, BinaryData::IBMPlexSansRegular_ttfSize);
    return typeface;
}

juce::Typeface::Ptr plexSansSemibold()
{
    static const auto typeface =
        load(BinaryData::IBMPlexSansSemiBold_ttf, BinaryData::IBMPlexSansSemiBold_ttfSize);
    return typeface;
}

juce::Typeface::Ptr plexMono()
{
    static const auto typeface =
        load(BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
    return typeface;
}

}  // namespace

juce::Font sans(float height, bool semibold)
{
    return juce::Font(
        juce::FontOptions(semibold ? plexSansSemibold() : plexSans()).withHeight(height));
}

juce::Font mono(float height)
{
    return juce::Font(juce::FontOptions(plexMono()).withHeight(height));
}

float textWidth(const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox(0, -1, true).getWidth();
}

}  // namespace polylogue::ui
