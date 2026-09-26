#pragma once

#include "dsp/Settings.h"

#include <cstdint>

namespace polylogue::dsp {

// Two-stage envelope in the monologue's manner: attack and decay only, with a type that decides
// how the gate is treated.
class Envelope {
public:
    void setParameters(const EnvelopeSettings& settings, double sampleRate);

    void noteOn();
    void noteOff();
    void reset();

    float next();
    bool isIdle() const { return stage_ == Stage::Idle; }

private:
    enum class Stage : std::uint8_t {
        Idle,
        Attack,
        Hold,
        Decay
    };

    EnvelopeType type_ = EnvelopeType::AGD;
    Stage stage_ = Stage::Idle;
    float level_ = 0.0f;
    float attackCoefficient_ = 1.0f;
    float decayCoefficient_ = 0.0f;
};

}  // namespace polylogue::dsp
