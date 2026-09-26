#include "ui/PresetMenu.h"

#include "presets/FactoryPresets.h"

#include <algorithm>
#include <cstdint>

namespace polylogue::ui::preset_menu {
namespace {

constexpr int kSaveItem = 100000;
constexpr int kDeleteItem = 100001;

int indexOfCurrent(const host::PresetManager& presets)
{
    const auto& entries = presets.entries();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name == presets.currentName())
            return static_cast<int>(i);
    }
    return -1;
}

}  // namespace

void showBrowser(juce::Component& anchor, host::PresetManager& presets, StatusCallback status)
{
    presets.rescanUserPresets();
    const auto entries = presets.entries();
    const int current = indexOfCurrent(presets);

    juce::PopupMenu menu;
    for (const char* category : presets::presetCategories()) {
        juce::PopupMenu submenu;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].isFactory() && entries[i].category == category)
                submenu.addItem(static_cast<int>(i) + 1, entries[i].name, true,
                                static_cast<int>(i) == current);
        }
        menu.addSubMenu(category, submenu);
    }

    juce::PopupMenu user;
    bool anyUser = false;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].isFactory()) {
            user.addItem(static_cast<int>(i) + 1, entries[i].name, true,
                         static_cast<int>(i) == current);
            anyUser = true;
        }
    }
    if (!anyUser)
        user.addItem(0, "No saved sounds yet", false);
    menu.addSubMenu("User", user);

    menu.addSeparator();
    menu.addItem(kSaveItem, "Save As...");
    const bool currentIsUser =
        current >= 0 && !entries[static_cast<std::size_t>(current)].isFactory();
    if (currentIsUser)
        menu.addItem(kDeleteItem, "Delete \"" + presets.currentName() + "\"");

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&anchor).withMinimumWidth(180),
        [&presets, entries, current, status,
         safe = juce::Component::SafePointer<juce::Component>(&anchor)](int choice) {
            if (safe == nullptr || choice == 0)
                return;
            if (choice == kSaveItem) {
                promptSave(*safe, presets, status);
            } else if (choice == kDeleteItem && current >= 0) {
                if (presets.deleteUser(entries[static_cast<std::size_t>(current)]) && status)
                    status("DELETED");
            } else if (choice >= 1 && static_cast<std::size_t>(choice) <= entries.size()) {
                presets.load(entries[static_cast<std::size_t>(choice) - 1]);
            }
        });
}

juce::AlertWindow* promptSave(juce::Component& owner, host::PresetManager& presets,
                              StatusCallback status)
{
    const int current = indexOfCurrent(presets);
    const bool editingUser =
        current >= 0 && !presets.entries()[static_cast<std::size_t>(current)].isFactory();
    const juce::String suggestion =
        editingUser ? presets.currentName()
                    : (presets.currentName() == "Init" ? juce::String("My Sound")
                                                       : presets.currentName() + " Copy");

    auto* window = new juce::AlertWindow("Save sound", "Name this sound.",
                                         juce::MessageBoxIconType::NoIcon, &owner);
    window->setLookAndFeel(&owner.getLookAndFeel());
    window->addTextEditor("name", suggestion);
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(
        true,
        juce::ModalCallbackFunction::create(
            [window, &presets, status,
             safe = juce::Component::SafePointer<juce::Component>(&owner)](int result) {
                if (safe == nullptr || result != 1)
                    return;
                const bool saved = presets.saveUser(window->getTextEditorContents("name"));
                if (status)
                    status(saved ? "SAVED" : "COULD NOT SAVE");
            }),
        true);
    return window;
}

void step(host::PresetManager& presets, int direction)
{
    const auto& entries = presets.entries();
    if (entries.empty())
        return;
    const int count = static_cast<int>(entries.size());
    const int current = indexOfCurrent(presets);
    const int next =
        current < 0 ? (direction > 0 ? 0 : count - 1) : (current + direction + count) % count;
    presets.load(entries[static_cast<std::size_t>(next)]);
}

}  // namespace polylogue::ui::preset_menu
