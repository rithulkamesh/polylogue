#include "host/MidiMapper.h"

#include <cstdint>
#include <utility>

namespace polylogue::host {
namespace {

using dsp::Param;

// From the monologue's MIDI implementation chart. Its single EG drives both of ours, so the
// attack, decay and type controllers map to the amp envelope and INT and TARGET to the mod
// envelope.
constexpr std::pair<Param, int> kDefaults[] = {
    {Param::Level, 7},
    {Param::AmpAttack, 16},
    {Param::AmpDecay, 17},
    {Param::LfoRate, 24},
    {Param::EnvInt, 25},
    {Param::LfoInt, 26},
    {Param::Drive, 28},
    {Param::Osc2Pitch, 35},
    {Param::Osc1Shape, 36},
    {Param::Osc2Shape, 37},
    {Param::Osc1Level, 39},
    {Param::Osc2Level, 40},
    {Param::Cutoff, 43},
    {Param::Resonance, 44},
    {Param::Osc2Octave, 49},
    {Param::Osc1Wave, 50},
    {Param::Osc2Wave, 51},
    {Param::LfoTarget, 56},
    {Param::LfoWave, 58},
    {Param::LfoMode, 59},
    {Param::Osc2SyncRing, 60},
    {Param::AmpType, 61},
    {Param::EnvTarget, 62},
    // The eight play knobs, on the eight generic sound controllers.
    {Param::AxisWave, 70},
    {Param::AxisMetal, 71},
    {Param::AxisGrit, 72},
    {Param::AxisBright, 73},
    {Param::AxisAttack, 74},
    {Param::AxisSustain, 75},
    {Param::AxisEvolve, 76},
    {Param::AxisMotion, 77},
};

bool validSlot(int slot)
{
    return slot >= 0 && slot < MidiMapper::kSlotCount;
}

}  // namespace

bool MidiMapper::isMappable(int controller)
{
    return controller >= 1 && controller < 120 && controller != 32 && controller != 64;
}

int MidiMapper::defaultController(Param param)
{
    for (const auto& [target, controller] : kDefaults) {
        if (target == param)
            return controller;
    }
    return kUnbound;
}

int MidiMapper::controllerFor(int slot) const
{
    return validSlot(slot)
               ? bindings_[static_cast<std::size_t>(slot)].load(std::memory_order_relaxed)
               : kUnbound;
}

void MidiMapper::bind(int slot, int controller)
{
    if (!validSlot(slot) || (controller != kUnbound && !isMappable(controller)))
        return;

    if (controller != kUnbound) {
        for (auto& other : bindings_) {
            if (other.load(std::memory_order_relaxed) == controller)
                other.store(kUnbound, std::memory_order_relaxed);
        }
    }
    bindings_[static_cast<std::size_t>(slot)].store(controller, std::memory_order_relaxed);
    version_.fetch_add(1, std::memory_order_relaxed);
}

void MidiMapper::resetToDefaults()
{
    for (auto& binding : bindings_)
        binding.store(kUnbound, std::memory_order_relaxed);
    for (const auto& [param, controller] : kDefaults)
        bindings_[dsp::index(param)].store(controller, std::memory_order_relaxed);
    learning_.store(kUnbound, std::memory_order_relaxed);
    version_.fetch_add(1, std::memory_order_relaxed);
}

void MidiMapper::startLearning(int slot)
{
    learning_.store(validSlot(slot) ? slot : kUnbound, std::memory_order_relaxed);
    version_.fetch_add(1, std::memory_order_relaxed);
}

void MidiMapper::stopLearning()
{
    learning_.store(kUnbound, std::memory_order_relaxed);
    version_.fetch_add(1, std::memory_order_relaxed);
}

int MidiMapper::slotFor(int controller)
{
    if (!isMappable(controller))
        return kUnbound;

    lastController_.store(controller, std::memory_order_relaxed);
    activity_.fetch_add(1, std::memory_order_relaxed);

    const int learning = learning_.load(std::memory_order_relaxed);
    if (learning != kUnbound) {
        bind(learning, controller);
        learning_.store(kUnbound, std::memory_order_relaxed);
        return learning;
    }

    for (int slot = 0; slot < kSlotCount; ++slot) {
        if (bindings_[static_cast<std::size_t>(slot)].load(std::memory_order_relaxed) == controller)
            return slot;
    }
    return kUnbound;
}

}  // namespace polylogue::host
