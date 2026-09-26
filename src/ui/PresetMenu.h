#pragma once

#include "host/PresetManager.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace polylogue::ui::preset_menu {

using StatusCallback = std::function<void(const juce::String&)>;

// Drops down the browser: presets by category, then the user's own, then save and delete.
void showBrowser(juce::Component& anchor, host::PresetManager& presets, StatusCallback status);

// Asks for a name and saves the current sound. The dialog dies with `owner`. Returns the dialog
// so the caller can dismiss it early.
juce::AlertWindow* promptSave(juce::Component& owner, host::PresetManager& presets,
                              StatusCallback status);

// Loads the previous (-1) or next (+1) preset in browser order, wrapping around.
void step(host::PresetManager& presets, int direction);

}  // namespace polylogue::ui::preset_menu
