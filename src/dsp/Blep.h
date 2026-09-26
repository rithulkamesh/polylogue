#pragma once

#include <cmath>

namespace polylogue::dsp {

// Band-limiting residuals: the difference between a step (or ramp) smoothed by a cubic B-spline
// kernel four samples wide and the ideal one. `x` is the sample time minus the event time, in
// samples; the residual is zero outside [-2, 2].

// Unit step. Multiply by the size of the jump.
inline double stepResidual(double x)
{
    const double u = std::abs(x);
    if (u >= 2.0)
        return 0.0;
    double magnitude;
    if (u >= 1.0) {
        const double v = 2.0 - u;
        magnitude = v * v * v * v / 24.0;
    } else {
        magnitude = 0.5 + (-4.0 * u + 2.0 * u * u * u - 0.75 * u * u * u * u) / 6.0;
    }
    return x < 0.0 ? magnitude : -magnitude;
}

// Unit change of slope per sample. Multiply by the size of the slope change.
inline double rampResidual(double x)
{
    const double u = std::abs(x);
    if (u >= 2.0)
        return 0.0;
    if (u >= 1.0) {
        const double v = 2.0 - u;
        return v * v * v * v * v / 120.0;
    }
    const double u2 = u * u;
    return 7.0 / 30.0 - 0.5 * u + (2.0 * u2 - 0.5 * u2 * u2 + 0.15 * u2 * u2 * u) / 6.0;
}

}  // namespace polylogue::dsp
