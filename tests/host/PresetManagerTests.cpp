#include "host/HostTestSupport.h"
#include "presets/FactoryPresets.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace polylogue;
using namespace polylogue::test;

namespace {

bool closeEnough(float a, float b)
{
    return std::abs(a - b) <= 1e-4f * std::max(1.0f, std::abs(a));
}

const host::PresetManager::Entry* find(const host::PresetManager& manager, const juce::String& name)
{
    for (const auto& entry : manager.entries()) {
        if (entry.name == name)
            return &entry;
    }
    return nullptr;
}

}  // namespace

TEST_CASE("the browser lists the factory presets first")
{
    Rig rig;
    const auto& entries = rig.processor.presets().entries();
    REQUIRE(entries.size() == presets::factoryPresets().size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        CHECK(entries[i].isFactory());
        CHECK(entries[i].factoryIndex == static_cast<int>(i));
        CHECK(entries[i].name == juce::String(presets::factoryPresets()[i].name));
    }
}

TEST_CASE("every factory preset loads exactly")
{
    Rig rig;
    auto& manager = rig.processor.presets();
    for (const auto& entry : manager.entries()) {
        INFO(entry.name);
        manager.load(entry);
        CHECK(manager.currentName() == entry.name);
        CHECK_FALSE(manager.isModified());

        const auto expected = presets::resolve(
            presets::factoryPresets()[static_cast<std::size_t>(entry.factoryIndex)]);
        const auto actual = manager.currentValues();
        for (std::size_t p = 0; p < dsp::kParamCount; ++p) {
            INFO(dsp::paramSpecs()[p].id);
            CHECK(closeEnough(actual.values[p], expected.values[p]));
        }
    }
}

TEST_CASE("loading a preset replaces the whole sound, not just the differences")
{
    Rig rig;
    auto& manager = rig.processor.presets();
    rig.set(dsp::Param::Drive, 0.9f);
    rig.set(dsp::Param::LfoRate, 0.9f);
    manager.loadFactory(0);
    // Warm Pad leaves drive at its default, so a leftover value would show.
    CHECK(rig.get(dsp::Param::Drive) == 0.0f);
}

TEST_CASE("editing after a load marks the preset as modified until it is loaded again")
{
    Rig rig;
    auto& manager = rig.processor.presets();
    manager.loadFactory(3);
    CHECK_FALSE(manager.isModified());

    rig.set(dsp::Param::Cutoff, rig.get(dsp::Param::Cutoff) * 0.5f);
    CHECK(manager.isModified());

    manager.loadFactory(3);
    CHECK_FALSE(manager.isModified());
}

TEST_CASE("a saved user preset comes back exactly")
{
    Rig rig;
    auto& manager = rig.processor.presets();
    manager.loadFactory(6);
    rig.set(dsp::Param::Cutoff, 1357.0f);
    rig.set(dsp::Param::Osc2Pitch, 123.0f);
    rig.set(dsp::Param::KeyMode, 1.0f);
    const auto saved = manager.currentValues();

    REQUIRE(manager.saveUser("My Sound"));
    CHECK(manager.currentName() == "My Sound");
    CHECK_FALSE(manager.isModified());

    const auto* entry = find(manager, "My Sound");
    REQUIRE(entry != nullptr);
    CHECK_FALSE(entry->isFactory());
    CHECK(entry->category == "User");
    CHECK(entry->file.existsAsFile());

    manager.loadFactory(0);
    manager.load(*find(manager, "My Sound"));
    const auto loaded = manager.currentValues();
    for (std::size_t p = 0; p < dsp::kParamCount; ++p) {
        INFO(dsp::paramSpecs()[p].id);
        CHECK(closeEnough(loaded.values[p], saved.values[p]));
    }
}

TEST_CASE("user presets are found by a new session in the same folder")
{
    ensureJuceInitialised();
    TempDirectory storage;
    {
        host::PolylogueProcessor first(optionsFor(storage.directory));
        REQUIRE(first.presets().saveUser("Persisted"));
    }
    host::PolylogueProcessor second(optionsFor(storage.directory));
    CHECK(find(second.presets(), "Persisted") != nullptr);
}

TEST_CASE("names are made safe for the file system, and empty names are refused")
{
    Rig rig;
    auto& manager = rig.processor.presets();
    CHECK_FALSE(manager.saveUser(""));
    CHECK_FALSE(manager.saveUser("   "));

    REQUIRE(manager.saveUser("a/b:c?"));
    const auto* entry = find(manager, "a/b:c?");
    REQUIRE(entry != nullptr);
    CHECK(entry->file.getParentDirectory().getFileName() == "Presets");
}

TEST_CASE("saving over an existing name replaces it")
{
    Rig rig;
    auto& manager = rig.processor.presets();
    rig.set(dsp::Param::Cutoff, 500.0f);
    REQUIRE(manager.saveUser("Twice"));
    rig.set(dsp::Param::Cutoff, 900.0f);
    REQUIRE(manager.saveUser("Twice"));

    int count = 0;
    for (const auto& entry : manager.entries())
        count += entry.name == "Twice" ? 1 : 0;
    CHECK(count == 1);

    manager.loadFactory(0);
    manager.load(*find(manager, "Twice"));
    CHECK(closeEnough(rig.get(dsp::Param::Cutoff), 900.0f));
}

TEST_CASE("user presets can be deleted, factory ones cannot")
{
    Rig rig;
    auto& manager = rig.processor.presets();
    REQUIRE(manager.saveUser("Doomed"));
    const auto entry = *find(manager, "Doomed");
    REQUIRE(manager.deleteUser(entry));
    CHECK(find(manager, "Doomed") == nullptr);
    CHECK_FALSE(entry.file.existsAsFile());

    CHECK_FALSE(manager.deleteUser(manager.entries().front()));
}

TEST_CASE("damaged or foreign preset files are skipped")
{
    Rig rig;
    const auto folder = rig.storage.directory.getChildFile("Presets");
    REQUIRE(folder.createDirectory().wasOk());
    REQUIRE(folder.getChildFile("broken.xml").replaceWithText("<Preset name=\"x\""));
    REQUIRE(folder.getChildFile("other.xml").replaceWithText("<Something/>"));
    REQUIRE(folder.getChildFile("notes.txt").replaceWithText("hello"));

    rig.processor.presets().rescanUserPresets();
    CHECK(rig.processor.presets().entries().size() == presets::factoryPresets().size());
}

TEST_CASE("a hand-written preset tolerates unknown and missing entries")
{
    Rig rig;
    const auto folder = rig.storage.directory.getChildFile("Presets");
    REQUIRE(folder.createDirectory().wasOk());
    REQUIRE(folder.getChildFile("hand.xml")
                .replaceWithText("<Preset name=\"Hand\"><Param id=\"cutoff\" value=\"2000\"/>"
                                 "<Param id=\"nonsense\" value=\"1\"/></Preset>"));

    auto& manager = rig.processor.presets();
    manager.rescanUserPresets();
    rig.set(dsp::Param::Drive, 0.8f);
    manager.load(*find(manager, "Hand"));
    CHECK(closeEnough(rig.get(dsp::Param::Cutoff), 2000.0f));
    CHECK(rig.get(dsp::Param::Drive) == 0.0f);
}

TEST_CASE("presets do not touch the controller map")
{
    Rig rig;
    rig.processor.midiMapper().bind(0, 33);
    rig.processor.presets().loadFactory(5);
    CHECK(rig.processor.midiMapper().controllerFor(0) == 33);
}

TEST_CASE("the host is told whenever the loaded sound or the preset list changes")
{
    Rig rig;
    auto& manager = rig.processor.presets();
    int calls = 0;
    manager.setChangeCallback([&calls] { ++calls; });

    manager.loadFactory(3);
    CHECK(calls == 1);

    REQUIRE(manager.saveUser("Announced"));
    CHECK(calls >= 2);

    const int afterSave = calls;
    manager.rescanUserPresets();  // nothing changed
    CHECK(calls == afterSave);

    REQUIRE(manager.deleteUser(*find(manager, "Announced")));
    CHECK(calls > afterSave);

    manager.setChangeCallback({});
}

TEST_CASE("a host listener hears that the program changed")
{
    struct Listener : juce::AudioProcessorListener {
        void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) override
        {
            programChanged = programChanged || details.programChanged;
        }
        bool programChanged = false;
    } listener;

    Rig rig;
    rig.processor.addListener(&listener);
    rig.processor.presets().loadFactory(8);
    CHECK(listener.programChanged);
    rig.processor.removeListener(&listener);
}

TEST_CASE("the host's program list includes the user's own sounds")
{
    Rig rig;
    auto& processor = rig.processor;
    const int factoryCount = processor.getNumPrograms();

    rig.set(dsp::Param::Cutoff, 1111.0f);
    REQUIRE(processor.presets().saveUser("My Program"));
    REQUIRE(processor.getNumPrograms() == factoryCount + 1);

    const int last = processor.getNumPrograms() - 1;
    CHECK(processor.getProgramName(last) == "My Program");
    CHECK(processor.getCurrentProgram() == last);

    processor.setCurrentProgram(0);
    CHECK(processor.presets().currentName() == juce::String(presets::factoryPresets()[0].name));
    processor.setCurrentProgram(last);
    CHECK(processor.presets().currentName() == "My Program");
    CHECK(closeEnough(rig.get(dsp::Param::Cutoff), 1111.0f));
}

TEST_CASE("a host rounding a value slightly does not make a sound look edited")
{
    Rig rig;
    auto& manager = rig.processor.presets();
    manager.loadFactory(2);
    REQUIRE_FALSE(manager.isModified());

    auto* cutoff = rig.processor.parameter(dsp::Param::Cutoff);
    cutoff->setValueNotifyingHost(cutoff->getValue() + 2e-4f);
    CHECK_FALSE(manager.isModified());

    cutoff->setValueNotifyingHost(cutoff->getValue() + 0.05f);
    CHECK(manager.isModified());
}
