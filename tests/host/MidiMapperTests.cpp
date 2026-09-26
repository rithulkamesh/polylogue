#include "host/MidiMapper.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <set>

using polylogue::dsp::Param;
using polylogue::host::MidiMapper;

namespace {

int slot(Param param)
{
    return MidiMapper::slotOf(param);
}

}  // namespace

TEST_CASE("every parameter is a slot")
{
    CHECK(MidiMapper::kSlotCount == static_cast<int>(polylogue::dsp::kParamCount));
}

TEST_CASE("controls start on the monologue's controller chart")
{
    MidiMapper mapper;
    CHECK(mapper.controllerFor(slot(Param::AmpAttack)) == 16);
    CHECK(mapper.controllerFor(slot(Param::AmpDecay)) == 17);
    CHECK(mapper.controllerFor(slot(Param::LfoRate)) == 24);
    CHECK(mapper.controllerFor(slot(Param::EnvInt)) == 25);
    CHECK(mapper.controllerFor(slot(Param::LfoInt)) == 26);
    CHECK(mapper.controllerFor(slot(Param::Drive)) == 28);
    CHECK(mapper.controllerFor(slot(Param::Osc2Pitch)) == 35);
    CHECK(mapper.controllerFor(slot(Param::Osc1Shape)) == 36);
    CHECK(mapper.controllerFor(slot(Param::Osc2Shape)) == 37);
    CHECK(mapper.controllerFor(slot(Param::Osc1Level)) == 39);
    CHECK(mapper.controllerFor(slot(Param::Osc2Level)) == 40);
    CHECK(mapper.controllerFor(slot(Param::Cutoff)) == 43);
    CHECK(mapper.controllerFor(slot(Param::Resonance)) == 44);
    CHECK(mapper.controllerFor(slot(Param::Osc2Octave)) == 49);
    CHECK(mapper.controllerFor(slot(Param::Osc1Wave)) == 50);
    CHECK(mapper.controllerFor(slot(Param::Osc2Wave)) == 51);
    CHECK(mapper.controllerFor(slot(Param::LfoTarget)) == 56);
    CHECK(mapper.controllerFor(slot(Param::LfoWave)) == 58);
    CHECK(mapper.controllerFor(slot(Param::LfoMode)) == 59);
    CHECK(mapper.controllerFor(slot(Param::Osc2SyncRing)) == 60);
    CHECK(mapper.controllerFor(slot(Param::EnvTarget)) == 62);
    CHECK(mapper.controllerFor(slot(Param::Level)) == 7);
}

TEST_CASE("parameters without a chart entry start unbound")
{
    MidiMapper mapper;
    for (Param param : {Param::Tune, Param::Polyphony, Param::GlideTime, Param::VelAmp})
        CHECK(mapper.controllerFor(slot(param)) == MidiMapper::kUnbound);
}

TEST_CASE("no two parameters share a default controller, and all are mappable")
{
    std::set<int> seen;
    for (std::size_t i = 0; i < polylogue::dsp::kParamCount; ++i) {
        const int controller = MidiMapper::defaultController(static_cast<Param>(i));
        if (controller == MidiMapper::kUnbound)
            continue;
        CHECK(MidiMapper::isMappable(controller));
        CHECK(seen.insert(controller).second);
    }
}

TEST_CASE("a controller drives the parameter it is bound to")
{
    MidiMapper mapper;
    CHECK(mapper.slotFor(43) == slot(Param::Cutoff));
    CHECK(mapper.slotFor(44) == slot(Param::Resonance));
    CHECK(mapper.slotFor(3) == MidiMapper::kUnbound);
}

TEST_CASE("binding moves a controller from any parameter that had it")
{
    MidiMapper mapper;
    mapper.bind(slot(Param::Tune), 43);
    CHECK(mapper.controllerFor(slot(Param::Tune)) == 43);
    CHECK(mapper.controllerFor(slot(Param::Cutoff)) == MidiMapper::kUnbound);
    CHECK(mapper.slotFor(43) == slot(Param::Tune));
}

TEST_CASE("a control can be cleared")
{
    MidiMapper mapper;
    mapper.bind(slot(Param::Cutoff), MidiMapper::kUnbound);
    CHECK(mapper.controllerFor(slot(Param::Cutoff)) == MidiMapper::kUnbound);
    CHECK(mapper.slotFor(43) == MidiMapper::kUnbound);
}

TEST_CASE("controllers with a fixed meaning cannot be mapped")
{
    for (int reserved : {0, 32, 64, 120, 121, 123, 127, 128, -5})
        CHECK_FALSE(MidiMapper::isMappable(reserved));
    for (int fine : {1, 7, 10, 74, 119})
        CHECK(MidiMapper::isMappable(fine));

    MidiMapper mapper;
    mapper.bind(slot(Param::Cutoff), 64);
    CHECK(mapper.controllerFor(slot(Param::Cutoff)) == 43);
    mapper.bind(slot(Param::Cutoff), 200);
    CHECK(mapper.controllerFor(slot(Param::Cutoff)) == 43);
    CHECK(mapper.slotFor(64) == MidiMapper::kUnbound);
}

TEST_CASE("out-of-range slots are ignored")
{
    MidiMapper mapper;
    mapper.bind(-1, 90);
    mapper.bind(MidiMapper::kSlotCount, 90);
    mapper.startLearning(MidiMapper::kSlotCount + 3);
    CHECK(mapper.controllerFor(-1) == MidiMapper::kUnbound);
    CHECK(mapper.controllerFor(MidiMapper::kSlotCount) == MidiMapper::kUnbound);
    CHECK(mapper.learningSlot() == MidiMapper::kUnbound);
    CHECK(mapper.slotFor(90) == MidiMapper::kUnbound);
}

TEST_CASE("learning binds the next controller that arrives")
{
    MidiMapper mapper;
    mapper.startLearning(slot(Param::Tune));
    CHECK(mapper.learningSlot() == slot(Param::Tune));

    CHECK(mapper.slotFor(21) == slot(Param::Tune));
    CHECK(mapper.controllerFor(slot(Param::Tune)) == 21);
    CHECK(mapper.learningSlot() == MidiMapper::kUnbound);

    CHECK(mapper.slotFor(21) == slot(Param::Tune));
    CHECK(mapper.slotFor(43) == slot(Param::Cutoff));
}

TEST_CASE("learning ignores controllers that cannot be mapped")
{
    MidiMapper mapper;
    mapper.startLearning(slot(Param::Tune));
    CHECK(mapper.slotFor(64) == MidiMapper::kUnbound);
    CHECK(mapper.learningSlot() == slot(Param::Tune));
    CHECK(mapper.slotFor(22) == slot(Param::Tune));
}

TEST_CASE("learning can be cancelled")
{
    MidiMapper mapper;
    mapper.startLearning(slot(Param::Tune));
    mapper.stopLearning();
    CHECK(mapper.slotFor(22) == MidiMapper::kUnbound);
    CHECK(mapper.controllerFor(slot(Param::Tune)) == MidiMapper::kUnbound);
}

TEST_CASE("resetting restores the chart and ends learning")
{
    MidiMapper mapper;
    mapper.bind(slot(Param::Cutoff), 90);
    mapper.bind(slot(Param::Tune), 91);
    mapper.startLearning(slot(Param::Drive));
    mapper.resetToDefaults();
    CHECK(mapper.controllerFor(slot(Param::Cutoff)) == 43);
    CHECK(mapper.controllerFor(slot(Param::Tune)) == MidiMapper::kUnbound);
    CHECK(mapper.learningSlot() == MidiMapper::kUnbound);
}

TEST_CASE("the display side can see changes and activity")
{
    MidiMapper mapper;
    const auto version = mapper.version();
    mapper.bind(slot(Param::Tune), 41);
    CHECK(mapper.version() != version);

    const auto activity = mapper.activity();
    mapper.slotFor(41);
    CHECK(mapper.activity() != activity);
    CHECK(mapper.lastController() == 41);

    const auto quiet = mapper.activity();
    mapper.slotFor(64);
    CHECK(mapper.activity() == quiet);
}
