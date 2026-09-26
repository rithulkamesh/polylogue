#pragma once

#include "dsp/Parameters.h"

#include <span>

namespace polylogue::ui {

enum class CellKind {
    Knob,
    ChipsColumn,  // a switch as a vertical stack of chips
    ChipsRow,     // a switch as a horizontal row of chips
};

struct Cell {
    dsp::Param param;
    const char* label;
    CellKind kind;
};

// A titled group of controls, laid out left to right on one row of the panel.
struct Section {
    const char* title;
    int row;
    std::span<const Cell> cells;
};

// The panel in the monologue's arrangement: master and drive, the oscillators, mixer, filter,
// the two envelopes, the LFO, then performance settings. Every parameter appears exactly once.
std::span<const Section> panelSections();

// One of the eight play knobs and what moving it does to the sound, low to high.
struct PlayKnob {
    dsp::Param param;
    const char* label;
    const char* hint;
};

// The play screen's knobs, in order. Their values are the eight axes; see dsp/Axes.h.
std::span<const PlayKnob> playKnobs();

// The label printed under a control, or the parameter's name in capitals.
const char* controlLabel(dsp::Param param);

}  // namespace polylogue::ui
