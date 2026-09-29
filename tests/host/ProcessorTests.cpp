#include "host/HostTestSupport.h"
#include "presets/FactoryPresets.h"
#include "support/AllocationGuard.h"
#include "support/Analysis.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

using namespace polylogue;
using namespace polylogue::test;

namespace {

juce::MidiBuffer noteOn(int note = 60, int at = 0)
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.8f), at);
    return midi;
}

juce::MemoryBlock stateOf(Rig& rig)
{
    juce::MemoryBlock block;
    rig.processor.getStateInformation(block);
    return block;
}

void loadState(Rig& rig, const juce::MemoryBlock& state)
{
    rig.processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
}

bool closeEnough(float a, float b)
{
    return std::abs(a - b) <= 1e-4f * std::max(1.0f, std::abs(a));
}

}  // namespace

TEST_CASE("every parameter is exposed with its stable id, default and range")
{
    Rig rig;
    for (const dsp::ParamSpec& spec : dsp::paramSpecs()) {
        INFO(spec.id);
        auto* parameter = rig.processor.parameters().getParameter(spec.id);
        REQUIRE(parameter != nullptr);
        CHECK(parameter->getName(64) == juce::String(spec.name));
        CHECK(closeEnough(parameter->convertFrom0to1(parameter->getDefaultValue()),
                          spec.defaultValue));
        CHECK(parameter->isAutomatable() == spec.automatable);
    }
    CHECK(rig.processor.getParameters().size() == static_cast<int>(dsp::kParamCount));
}

TEST_CASE("a macro is assigned to a parameter and the assignment is saved with the session")
{
    Rig rig;
    CHECK(rig.processor.macroTarget(0) == dsp::Param::Cutoff);
    CHECK(rig.processor.macroTarget(7) == dsp::Param::ChorusMix);

    rig.processor.assignMacro(0, dsp::Param::Osc2Pitch);
    rig.processor.assignMacro(7, dsp::Param::Level);
    CHECK(rig.processor.macroTarget(0) == dsp::Param::Osc2Pitch);
    CHECK(rig.processor.macroTarget(7) == dsp::Param::Level);
    CHECK(rig.processor.macroTarget(1) == dsp::Param::Resonance);

    Rig restored;
    loadState(restored, stateOf(rig));
    CHECK(restored.processor.macroTarget(0) == dsp::Param::Osc2Pitch);
    CHECK(restored.processor.macroTarget(7) == dsp::Param::Level);
}

TEST_CASE("assigning a macro does not change the sound")
{
    Rig rig;
    const dsp::ParamValues before = rig.processor.presets().currentValues();
    rig.processor.assignMacro(3, dsp::Param::Drive);
    dsp::ParamValues after = rig.processor.presets().currentValues();
    after[dsp::Param::Macro4] = before[dsp::Param::Macro4];
    CHECK(after.values == before.values);
}

TEST_CASE("host normalisation agrees with the parameter table")
{
    Rig rig;
    for (const dsp::ParamSpec& spec : dsp::paramSpecs()) {
        INFO(spec.id);
        auto* parameter = rig.processor.parameters().getParameter(spec.id);
        // Away from exact half-steps, where rounding is legitimately ambiguous.
        for (float n : {0.0f, 0.31f, 0.52f, 0.93f, 1.0f}) {
            const float expected = dsp::toPlain(spec, n);
            const auto& range = parameter->getNormalisableRange();
            CHECK(closeEnough(range.snapToLegalValue(parameter->convertFrom0to1(n)), expected));
            CHECK(std::abs(parameter->convertTo0to1(expected) - dsp::toNormalized(spec, expected)) <
                  1e-4f);
        }
    }
}

TEST_CASE("parameter text round-trips through the host")
{
    Rig rig;
    for (const dsp::ParamSpec& spec : dsp::paramSpecs()) {
        INFO(spec.id);
        auto* parameter = rig.processor.parameters().getParameter(spec.id);
        for (float n : {0.0f, 0.5f, 1.0f}) {
            // Integers and choices can only sit on whole steps.
            const auto& range = parameter->getNormalisableRange();
            const float legal =
                parameter->convertTo0to1(range.snapToLegalValue(parameter->convertFrom0to1(n)));

            const juce::String text = parameter->getText(legal, 0);
            CHECK(text.isNotEmpty());
            CHECK(std::abs(parameter->getValueForText(text) - legal) < 0.02f);
        }
    }
}

TEST_CASE("a saved session restores every parameter")
{
    std::mt19937 rng(77);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    Rig source;
    for (const dsp::ParamSpec& spec : dsp::paramSpecs())
        source.processor.parameters().getParameter(spec.id)->setValueNotifyingHost(unit(rng));
    const auto expected = source.processor.presets().currentValues();

    Rig restored;
    loadState(restored, stateOf(source));
    const auto actual = restored.processor.presets().currentValues();

    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        INFO(dsp::paramSpecs()[i].id);
        CHECK(closeEnough(actual.values[i], expected.values[i]));
    }
}

TEST_CASE("loading a session twice changes nothing")
{
    Rig a;
    a.set(dsp::Param::Cutoff, 1234.5f);
    a.set(dsp::Param::Osc2Pitch, -333.0f);
    const auto once = stateOf(a);

    Rig b;
    loadState(b, once);
    Rig c;
    loadState(c, stateOf(b));
    for (std::size_t i = 0; i < dsp::kParamCount; ++i)
        CHECK(closeEnough(c.processor.presets().currentValues().values[i],
                          a.processor.presets().currentValues().values[i]));
}

TEST_CASE("unknown ids are ignored and missing ones return to their defaults")
{
    juce::XmlElement xml("Polylogue");
    xml.setAttribute("version", 99);
    auto* known = xml.createNewChildElement("Param");
    known->setAttribute("id", "cutoff");
    known->setAttribute("value", "1234");
    auto* unknown = xml.createNewChildElement("Param");
    unknown->setAttribute("id", "from_the_future");
    unknown->setAttribute("value", "5");
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(xml, block);

    Rig rig;
    rig.set(dsp::Param::Resonance, 0.9f);
    loadState(rig, block);

    CHECK(closeEnough(rig.get(dsp::Param::Cutoff), 1234.0f));
    CHECK(rig.get(dsp::Param::Resonance) == 0.0f);
}

TEST_CASE("out-of-range saved values are clamped")
{
    juce::XmlElement xml("Polylogue");
    auto* bad = xml.createNewChildElement("Param");
    bad->setAttribute("id", "cutoff");
    bad->setAttribute("value", "999999");
    auto* worse = xml.createNewChildElement("Param");
    worse->setAttribute("id", "osc1_wave");
    worse->setAttribute("value", "-7");
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(xml, block);

    Rig rig;
    loadState(rig, block);
    CHECK(rig.get(dsp::Param::Cutoff) == 20000.0f);
    CHECK(rig.get(dsp::Param::Osc1Wave) == 0.0f);
}

TEST_CASE("garbage state is rejected without disturbing the sound")
{
    Rig rig;
    rig.set(dsp::Param::Cutoff, 777.0f);
    const char junk[] = "this is not a session";
    rig.processor.setStateInformation(junk, sizeof junk);
    rig.processor.setStateInformation(nullptr, 0);

    juce::XmlElement other("SomethingElse");
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(other, block);
    loadState(rig, block);

    CHECK(closeEnough(rig.get(dsp::Param::Cutoff), 777.0f));
}

TEST_CASE("the loaded preset name and its edited state survive a session")
{
    Rig source;
    source.processor.setCurrentProgram(4);
    const juce::String name = source.processor.presets().currentName();
    REQUIRE(name.isNotEmpty());

    Rig untouched;
    loadState(untouched, stateOf(source));
    CHECK(untouched.processor.presets().currentName() == name);
    CHECK_FALSE(untouched.processor.presets().isModified());

    source.set(dsp::Param::Cutoff, 333.0f);
    Rig edited;
    loadState(edited, stateOf(source));
    CHECK(edited.processor.presets().currentName() == name);
    CHECK(edited.processor.presets().isModified());
}

TEST_CASE("the session's controller map is a fallback for machines without one")
{
    Rig source;
    source.processor.midiMapper().bind(host::MidiMapper::slotOf(dsp::Param::Tune), 33);
    const auto state = stateOf(source);

    Rig fresh;
    loadState(fresh, state);
    CHECK(fresh.processor.midiMapper().controllerFor(host::MidiMapper::slotOf(dsp::Param::Tune)) ==
          33);
}

TEST_CASE("this machine's controller map wins over a session's")
{
    Rig source;
    source.processor.midiMapper().bind(host::MidiMapper::slotOf(dsp::Param::Tune), 33);
    const auto state = stateOf(source);

    TempDirectory storage;
    juce::XmlElement map("MidiMap");
    map.setAttribute("version", 2);
    auto* bind = map.createNewChildElement("Bind");
    bind->setAttribute("id", "tune");
    bind->setAttribute("cc", 44);
    REQUIRE(map.writeTo(storage.directory.getChildFile("midi-map.xml")));

    host::PolylogueProcessor local(optionsFor(storage.directory));
    local.prepareToPlay(Rig::kSampleRate, 512);
    local.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(local.midiMapper().controllerFor(host::MidiMapper::slotOf(dsp::Param::Tune)) == 44);
}

TEST_CASE("a map file from an older version is ignored")
{
    TempDirectory storage;
    juce::XmlElement map("MidiMap");
    map.setAttribute("knob0", 99);
    REQUIRE(map.writeTo(storage.directory.getChildFile("midi-map.xml")));

    ensureJuceInitialised();
    host::PolylogueProcessor processor(optionsFor(storage.directory));
    CHECK(processor.midiMapper().controllerFor(host::MidiMapper::slotOf(dsp::Param::Cutoff)) == 43);
}

TEST_CASE("the controller map persists between runs")
{
    ensureJuceInitialised();
    TempDirectory storage;
    using host::MidiMapper;
    {
        host::PolylogueProcessor first(optionsFor(storage.directory));
        first.midiMapper().bind(MidiMapper::slotOf(dsp::Param::Tune), 55);
        first.midiMapper().bind(MidiMapper::slotOf(dsp::Param::Cutoff), MidiMapper::kUnbound);
    }
    host::PolylogueProcessor second(optionsFor(storage.directory));
    CHECK(second.midiMapper().controllerFor(MidiMapper::slotOf(dsp::Param::Tune)) == 55);
    CHECK(second.midiMapper().controllerFor(MidiMapper::slotOf(dsp::Param::Cutoff)) ==
          MidiMapper::kUnbound);
    CHECK(second.midiMapper().controllerFor(MidiMapper::slotOf(dsp::Param::Resonance)) == 44);
}

TEST_CASE("notes make sound and silence makes none")
{
    Rig rig;
    CHECK(peak(rig.run()) == 0.0f);

    auto midi = noteOn();
    rig.run(midi);
    CHECK(rms(rig.run()) > 0.01);
}

TEST_CASE("a note starts on its sample inside the block")
{
    Rig rig;
    auto midi = noteOn(60, 200);
    const auto out = rig.run(midi);
    for (int i = 0; i < 200; ++i)
        REQUIRE(out[static_cast<std::size_t>(i)] == 0.0f);
    CHECK(peak(std::span<const float>(out).subspan(200)) > 0.0f);
}

TEST_CASE("both output channels carry the same audio")
{
    Rig rig;
    auto midi = noteOn();
    juce::AudioBuffer<float> buffer(2, 512);
    rig.processor.processBlock(buffer, midi);
    juce::AudioBuffer<float> again(2, 512);
    juce::MidiBuffer none;
    rig.processor.processBlock(again, none);
    for (int i = 0; i < 512; ++i)
        REQUIRE(again.getSample(0, i) == again.getSample(1, i));
}

TEST_CASE("unusual buffer shapes are handled")
{
    Rig rig;
    for (auto [channels, samples] : {std::pair{1, 64}, std::pair{2, 1}, std::pair{2, 8192},
                                     std::pair{4, 128}, std::pair{2, 0}}) {
        juce::AudioBuffer<float> buffer(channels, samples);
        buffer.clear();
        auto midi = noteOn();
        rig.processor.processBlock(buffer, midi);
        for (int c = 0; c < channels; ++c) {
            for (int i = 0; i < samples; ++i)
                REQUIRE(std::isfinite(buffer.getSample(c, i)));
        }
        if (channels > 2 && samples > 0)
            CHECK(buffer.getSample(3, 0) == 0.0f);
    }
}

TEST_CASE("a controller moves its control in the sound at once and in the parameter soon after")
{
    auto brightness = [](bool sendController) {
        Rig rig;
        rig.set(dsp::Param::Cutoff, 100.0f);
        juce::MidiBuffer midi = noteOn();
        if (sendController)
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 43, 127), 0);
        rig.run(midi);
        double level = 0.0;
        for (int i = 0; i < 4; ++i)
            level += rms(rig.run());

        if (sendController) {
            CHECK(rig.get(dsp::Param::Cutoff) == 100.0f);  // not delivered yet
            rig.processor.processPendingControlChanges();
            CHECK(closeEnough(rig.get(dsp::Param::Cutoff), 20000.0f));
        }
        return level;
    };
    CHECK(brightness(true) > 2.0 * brightness(false));
}

TEST_CASE("controller values span the whole control")
{
    Rig rig;
    for (auto [value, expected] : {std::pair{0, 20.0f}, std::pair{127, 20000.0f}}) {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 43, value), 0);
        rig.run(midi);
        rig.processor.processPendingControlChanges();
        CHECK(closeEnough(rig.get(dsp::Param::Cutoff), expected));
    }
}

TEST_CASE("controllers select switch positions")
{
    Rig rig;
    for (auto [value, expected] : {std::pair{0, 0.0f}, std::pair{64, 1.0f}, std::pair{127, 2.0f}}) {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 50, value), 0);  // oscillator 1 wave
        rig.run(midi);
        rig.processor.processPendingControlChanges();
        CHECK(rig.get(dsp::Param::Osc1Wave) == expected);
    }
}

TEST_CASE("a switch moved by a controller changes the sound at once")
{
    Rig rig;
    juce::MidiBuffer midi = noteOn(45);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 50, 127), 0);  // square
    rig.run(midi);
    // A square wave holds its level; a saw ramps. Compare how much the signal moves between
    // samples.
    const auto out = rig.run();
    double flat = 0.0;
    for (std::size_t i = 1; i < out.size(); ++i)
        flat += std::abs(out[i] - out[i - 1]) < 0.01f ? 1.0 : 0.0;
    CHECK(flat / static_cast<double>(out.size()) > 0.5);
}

TEST_CASE("controllers without a binding are ignored")
{
    Rig rig;
    const float before = rig.get(dsp::Param::Cutoff);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 3, 100), 0);
    rig.run(midi);
    rig.processor.processPendingControlChanges();
    CHECK(rig.get(dsp::Param::Cutoff) == before);
}

TEST_CASE("learning binds a hardware knob to any control")
{
    Rig rig;
    const int slot = host::MidiMapper::slotOf(dsp::Param::Tune);
    rig.processor.midiMapper().startLearning(slot);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 21, 127), 0);
    rig.run(midi);
    CHECK(rig.processor.midiMapper().controllerFor(slot) == 21);

    rig.processor.processPendingControlChanges();
    CHECK(closeEnough(rig.get(dsp::Param::Tune), 50.0f));

    juce::MidiBuffer more;
    more.addEvent(juce::MidiMessage::controllerEvent(1, 21, 0), 0);
    rig.run(more);
    rig.processor.processPendingControlChanges();
    CHECK(closeEnough(rig.get(dsp::Param::Tune), -50.0f));
}

TEST_CASE("every default controller reaches its own parameter")
{
    Rig rig;
    int tested = 0;
    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        const int controller = host::MidiMapper::defaultController(static_cast<dsp::Param>(i));
        if (controller == host::MidiMapper::kUnbound)
            continue;
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, controller, 127), 0);
        rig.run(midi);
        ++tested;
    }
    rig.processor.processPendingControlChanges();

    CHECK(tested >= 20);
    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        const auto param = static_cast<dsp::Param>(i);
        if (host::MidiMapper::defaultController(param) == host::MidiMapper::kUnbound)
            continue;
        INFO(dsp::paramSpec(param).id);
        CHECK(closeEnough(rig.get(param), dsp::paramSpec(param).max));
    }
}

TEST_CASE("sustain pedal and pitch bend reach the engine")
{
    Rig rig;
    juce::MidiBuffer midi = noteOn();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 1);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 2);
    rig.run(midi);
    for (int i = 0; i < 10; ++i)
        rig.run();
    CHECK(rms(rig.run()) > 0.01);  // held by the pedal

    juce::MidiBuffer lift;
    lift.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
    rig.run(lift);
    for (int i = 0; i < 200; ++i)
        rig.run();
    CHECK(peak(rig.run()) == 0.0f);
}

TEST_CASE("the activity counter and the scope follow the traffic")
{
    Rig rig;
    const auto before = rig.processor.midiActivity();
    auto midi = noteOn();
    rig.run(midi);
    CHECK(rig.processor.midiActivity() != before);

    const auto quiet = rig.processor.midiActivity();
    rig.run();
    CHECK(rig.processor.midiActivity() == quiet);

    std::vector<float> latest(256);
    rig.processor.scope().readLatest(latest.data(), latest.size());
    CHECK(peak(latest) > 0.0f);
}

TEST_CASE("notes from the on-screen keyboard play and release")
{
    Rig rig;
    rig.processor.queueKeyboardNote(60, 0.8f);
    rig.run();
    CHECK(rms(rig.run()) > 0.01);

    rig.processor.queueKeyboardNote(60, 0.0f);
    for (int i = 0; i < 200; ++i)
        rig.run();
    CHECK(peak(rig.run()) == 0.0f);
}

TEST_CASE("keyboard notes start at the top of the block, ahead of the host's MIDI")
{
    Rig rig;
    rig.processor.queueKeyboardNote(48, 0.9f);
    auto midi = noteOn(64, 300);
    const auto out = rig.run(midi);
    CHECK(peak(std::span<const float>(out).subspan(0, 250)) > 0.0f);
}

TEST_CASE("hardware notes are reported back to the display, keyboard notes are not")
{
    Rig rig;
    juce::MidiBuffer midi = noteOn(64, 10);
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 200);
    rig.run(midi);

    host::PolylogueProcessor::KeyEvent event;
    REQUIRE(rig.processor.popMonitoredNote(event));
    CHECK(event.note == 64);
    CHECK(event.velocity > 0.0f);
    REQUIRE(rig.processor.popMonitoredNote(event));
    CHECK(event.note == 64);
    CHECK(event.velocity == 0.0f);
    CHECK_FALSE(rig.processor.popMonitoredNote(event));

    rig.processor.queueKeyboardNote(50, 0.8f);
    rig.run();
    CHECK_FALSE(rig.processor.popMonitoredNote(event));
}

TEST_CASE("a flood of keyboard notes is handled without overflow")
{
    Rig rig;
    for (int i = 0; i < 1000; ++i)
        rig.processor.queueKeyboardNote(30 + i % 60, i % 2 == 0 ? 0.7f : 0.0f);
    for (int i = 0; i < 20; ++i) {
        const auto out = rig.run();
        REQUIRE(allFinite(out));
    }
}

TEST_CASE("programs are the factory presets")
{
    Rig rig;
    const auto factory = presets::factoryPresets();
    REQUIRE(rig.processor.getNumPrograms() == static_cast<int>(factory.size()));

    for (int i : {0, 7, static_cast<int>(factory.size()) - 1}) {
        CHECK(rig.processor.getProgramName(i) ==
              juce::String(factory[static_cast<std::size_t>(i)].name));
        rig.processor.setCurrentProgram(i);
        CHECK(rig.processor.getCurrentProgram() == i);

        const auto expected = presets::resolve(factory[static_cast<std::size_t>(i)]);
        const auto actual = rig.processor.presets().currentValues();
        for (std::size_t p = 0; p < dsp::kParamCount; ++p)
            CHECK(closeEnough(actual.values[p], expected.values[p]));
    }
    CHECK(rig.processor.getProgramName(-1).isEmpty());
    CHECK(rig.processor.getProgramName(9999).isEmpty());
}

TEST_CASE("the audio path never allocates")
{
    if (!allocationTrackingAvailable()) {
        SUCCEED("allocation tracking is unavailable under this sanitizer");
        return;
    }

    Rig rig;
    rig.processor.setCurrentProgram(0);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer warmUp = noteOn();
    rig.processor.processBlock(buffer, warmUp);

    std::vector<juce::MidiBuffer> blocks(8);
    blocks[0] = noteOn(55, 10);
    blocks[1].addEvent(juce::MidiMessage::noteOn(1, 62, 0.9f), 0);
    blocks[1].addEvent(juce::MidiMessage::controllerEvent(1, 43, 90), 100);
    blocks[1].addEvent(juce::MidiMessage::controllerEvent(1, 44, 30), 200);
    blocks[2].addEvent(juce::MidiMessage::pitchWheel(1, 12000), 5);
    blocks[3].addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    blocks[4].addEvent(juce::MidiMessage::noteOff(1, 55), 30);
    blocks[5].addEvent(juce::MidiMessage::allNotesOff(1), 0);
    blocks[6].addEvent(juce::MidiMessage::allSoundOff(1), 0);
    for (int note = 40; note < 60; ++note)
        rig.processor.queueKeyboardNote(note, note % 2 == 0 ? 0.8f : 0.0f);

    AllocationGuard guard;
    for (int round = 0; round < 4; ++round) {
        for (auto& midi : blocks) {
            buffer.clear();
            rig.processor.processBlock(buffer, midi);
        }
    }
    CHECK(guard.count() == 0);
}
