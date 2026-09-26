#include "dsp/Engine.h"
#include "presets/FactoryPresets.h"
#include "support/AllocationGuard.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace polylogue;
using namespace polylogue::test;

TEST_CASE("rendering audio never allocates, for any preset")
{
    if (!allocationTrackingAvailable()) {
        SUCCEED("allocation tracking is unavailable under this sanitizer");
        return;
    }

    for (const auto& preset : presets::factoryPresets()) {
        INFO(preset.name);
        dsp::Engine engine;
        engine.setSettings(dsp::toSettings(presets::resolve(preset)));
        engine.prepare(48000.0);

        std::vector<float> left(512);
        std::vector<float> right(512);
        const std::vector<dsp::MidiEvent> events = {
            dsp::MidiEvent::noteOn(0, 48, 0.8f),   dsp::MidiEvent::noteOn(10, 55, 0.6f),
            dsp::MidiEvent::pitchBend(20, 0.4f),   dsp::MidiEvent::sustain(30, true),
            dsp::MidiEvent::noteOff(100, 48),      dsp::MidiEvent::noteOn(200, 60, 1.0f),
            dsp::MidiEvent::noteOn(300, 60, 0.5f), dsp::MidiEvent::allNotesOff(400),
        };

        AllocationGuard guard;
        for (int block = 0; block < 40; ++block) {
            engine.process(block == 0 ? std::span<const dsp::MidiEvent>(events)
                                      : std::span<const dsp::MidiEvent>(),
                           left.data(), right.data(), 512);
        }
        CHECK(guard.count() == 0);
    }
}

TEST_CASE("the allocation guard notices allocations")
{
    if (!allocationTrackingAvailable()) {
        SUCCEED("allocation tracking is unavailable under this sanitizer");
        return;
    }
    AllocationGuard guard;
    auto* block = new std::vector<float>(1000);
    // An optimizer may delete an unused allocation, so make this one observable.
    asm volatile("" : : "r"(block->data()) : "memory");
    CHECK(guard.count() >= 1);
    delete block;
}
