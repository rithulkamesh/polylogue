#pragma once

#include "dsp/Smoother.h"

#include <array>
#include <cstddef>

namespace polylogue::dsp {

// A stereo chorus: three modulated delay taps per side, spread evenly around the LFO cycle, which
// thickens a mono signal into a slowly swirling stereo one. At zero mix the input passes through
// untouched. Nothing allocates.
class Chorus {
public:
    void prepare(double sampleRate);
    void reset();

    // `rateHz` is the modulation speed; `depth` in [0, 1] scales the delay swing.
    void setParameters(float mix, float rateHz, float depth);

    void process(float input, float& left, float& right);

private:
    static constexpr std::size_t kTaps = 3;
    static constexpr std::size_t kBufferSize = 16384;  // 85 ms at 192 kHz

    float readTap(double delaySamples) const;

    std::array<float, kBufferSize> buffer_{};
    std::size_t writeIndex_ = 0;
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    double phaseIncrement_ = 0.0;
    float depth_ = 0.0f;
    Smoother mix_;
};

}  // namespace polylogue::dsp
