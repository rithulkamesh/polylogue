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

// The label printed under a control, or the parameter's name in capitals.
const char* controlLabel(dsp::Param param);

}  // namespace polylogue::ui
