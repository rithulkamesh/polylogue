#pragma once

#include "dsp/Parameters.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace polylogue::host {

// Binds MIDI controller numbers to parameters. Every parameter is a slot, so any control on the
// panel can be driven from hardware. Safe to use from the message thread and the audio thread at
// once: everything is a relaxed atomic, nothing locks.
class MidiMapper {
public:
    static constexpr int kSlotCount = static_cast<int>(dsp::kParamCount);
    static constexpr int kUnbound = -1;

    MidiMapper() { resetToDefaults(); }

    // Bank select, sustain and the channel-mode messages keep their standard meaning.
    static bool isMappable(int controller);
    // The monologue's own controller chart, plus CC 7 for level; other parameters start unbound.
    static int defaultController(dsp::Param param);

    static int slotOf(dsp::Param param) { return static_cast<int>(dsp::index(param)); }

    int controllerFor(int slot) const;
    // Binding a controller that another slot already uses moves it to this slot.
    void bind(int slot, int controller);
    void resetToDefaults();

    // The next mappable controller to arrive is bound to `slot`.
    void startLearning(int slot);
    void stopLearning();
    int learningSlot() const { return learning_.load(std::memory_order_relaxed); }

    // Audio thread: which slot a control change drives, or kUnbound. Completes a pending learn.
    int slotFor(int controller);

    // For the display: bumps whenever the map changes, and on every mappable controller seen.
    std::uint32_t version() const { return version_.load(std::memory_order_relaxed); }
    std::uint32_t activity() const { return activity_.load(std::memory_order_relaxed); }
    int lastController() const { return lastController_.load(std::memory_order_relaxed); }

private:
    std::array<std::atomic<int>, dsp::kParamCount> bindings_{};
    std::atomic<int> learning_{kUnbound};
    std::atomic<std::uint32_t> version_{0};
    std::atomic<std::uint32_t> activity_{0};
    std::atomic<int> lastController_{kUnbound};
};

}  // namespace polylogue::host
