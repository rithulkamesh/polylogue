#include "ui/PanelLayout.h"

#include <array>
#include <cctype>
#include <string>

namespace polylogue::ui {
namespace {

using dsp::Param;
constexpr CellKind K = CellKind::Knob;
constexpr CellKind C = CellKind::ChipsColumn;
constexpr CellKind R = CellKind::ChipsRow;

constexpr Cell kMaster[] = {{Param::Level, "MASTER", K}, {Param::Drive, "DRIVE", K}};

constexpr Cell kVco1[] = {{Param::Osc1Wave, "WAVE", C}, {Param::Osc1Shape, "SHAPE", K}};

constexpr Cell kVco2[] = {
    {Param::Osc2Octave, "OCTAVE", C},      {Param::Osc2Wave, "WAVE", C},
    {Param::Osc2SyncRing, "SYNC/RING", C}, {Param::Osc2Pitch, "PITCH", K},
    {Param::Osc2Shape, "SHAPE", K},
};

constexpr Cell kMixer[] = {{Param::Osc1Level, "VCO 1", K}, {Param::Osc2Level, "VCO 2", K}};

constexpr Cell kFilter[] = {
    {Param::Cutoff, "CUTOFF", K},
    {Param::Resonance, "RESONANCE", K},
    {Param::KeyTrack, "KEY TRK", C},
    {Param::VelCutoff, "VEL>CUT", K},
};

constexpr Cell kAmp[] = {
    {Param::AmpType, "TYPE", C},
    {Param::AmpAttack, "ATTACK", K},
    {Param::AmpDecay, "DECAY", K},
    {Param::VelAmp, "VEL>AMP", K},
};

constexpr Cell kMod[] = {
    {Param::EnvType, "TYPE", C}, {Param::EnvAttack, "ATTACK", K}, {Param::EnvDecay, "DECAY", K},
    {Param::EnvInt, "INT", K},   {Param::EnvTarget, "TARGET", C},
};

constexpr Cell kLfo[] = {
    {Param::LfoWave, "WAVE", C}, {Param::LfoMode, "MODE", C},     {Param::LfoRate, "RATE", K},
    {Param::LfoInt, "INT", K},   {Param::LfoTarget, "TARGET", C},
};

constexpr Cell kPlay[] = {{Param::KeyMode, "MODE", C}, {Param::Polyphony, "VOICES", K}};

constexpr Cell kChorus[] = {
    {Param::ChorusMix, "MIX", K},
    {Param::ChorusRate, "RATE", K},
    {Param::ChorusDepth, "DEPTH", K},
};

constexpr Cell kPerformance[] = {
    {Param::Octave, "OCTAVE", R},        {Param::Tune, "TUNE", K},
    {Param::BendRange, "BEND", K},       {Param::GlideTime, "GLIDE", K},
    {Param::GlideMode, "GLIDE MODE", R},
};

constexpr std::array kSections = {
    Section{"MASTER", 0, kMaster},       Section{"VCO 1", 0, kVco1},    Section{"VCO 2", 0, kVco2},
    Section{"MIXER", 0, kMixer},         Section{"FILTER", 0, kFilter}, Section{"AMP EG", 1, kAmp},
    Section{"MOD EG", 1, kMod},          Section{"LFO", 1, kLfo},       Section{"PLAY", 1, kPlay},
    Section{"PERFORM", 2, kPerformance}, Section{"CHORUS", 2, kChorus},
};

}  // namespace

std::span<const Section> panelSections()
{
    return kSections;
}

const char* controlLabel(Param param)
{
    for (const Section& section : kSections) {
        for (const Cell& cell : section.cells) {
            if (cell.param == param)
                return cell.label;
        }
    }
    return "";
}

}  // namespace polylogue::ui
