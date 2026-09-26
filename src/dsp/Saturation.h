#pragma once

#include <algorithm>
#include <cmath>

namespace polylogue::dsp {

// Rational tanh approximation, exact at the clamp points. Good to about 2%.
inline float fastTanh(float x)
{
    x = std::clamp(x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Identity up to `threshold`, then a smooth tanh knee that tops out at `ceiling`. Stays exactly
// linear at nominal levels, which a plain tanh does not.
inline float softLimit(float x, float threshold, float ceiling)
{
    const float magnitude = std::abs(x);
    if (magnitude <= threshold)
        return x;
    const float headroom = ceiling - threshold;
    return std::copysign(threshold + headroom * fastTanh((magnitude - threshold) / headroom), x);
}

}  // namespace polylogue::dsp
