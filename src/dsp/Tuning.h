#pragma once

#include <cmath>

namespace polylogue::dsp {

inline constexpr double kConcertPitchHz = 440.0;
inline constexpr double kConcertPitchNote = 69.0;

inline double semitonesToRatio(double semitones)
{
    return std::exp2(semitones / 12.0);
}

inline double midiToHz(double note)
{
    return kConcertPitchHz * semitonesToRatio(note - kConcertPitchNote);
}

inline float dbToGain(float db)
{
    return std::pow(10.0f, db * 0.05f);
}

}  // namespace polylogue::dsp
