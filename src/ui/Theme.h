#pragma once

#include <juce_graphics/juce_graphics.h>

// The visual language, shared with the sibling chorale plug-in: a near-black canvas, white ink,
// one neutral accent, IBM Plex.
namespace polylogue::ui {

inline const juce::Colour kBackground{0xff0a0a0a};
inline const juce::Colour kPanel{0xff121316};
inline const juce::Colour kPanelRaised{0xff1b1d21};
inline const juce::Colour kBorder{0xff26282d};
inline const juce::Colour kTrack{0xff2a2f36};
inline const juce::Colour kInk{0xffffffff};
inline const juce::Colour kDim{0xff8a9096};
inline const juce::Colour kAccent{0xffa6afb8};

// Embedded so the interface looks the same everywhere.
juce::Font sans(float height, bool semibold = false);
juce::Font mono(float height);

// Width of a single line of text in the given font.
float textWidth(const juce::Font& font, const juce::String& text);

}  // namespace polylogue::ui
