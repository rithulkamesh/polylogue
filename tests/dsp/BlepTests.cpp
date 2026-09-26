#include "dsp/Blep.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace polylogue::dsp;

TEST_CASE("step residual is odd and vanishes outside the kernel")
{
    for (double x = 0.05; x < 2.0; x += 0.1)
        CHECK(std::abs(stepResidual(x) + stepResidual(-x)) < 1e-12);
    CHECK(stepResidual(2.0) == 0.0);
    CHECK(stepResidual(-2.5) == 0.0);
}

TEST_CASE("smoothed step passes through one half at the event")
{
    // step + residual is the smoothed step, which is symmetric about (0, 1/2).
    CHECK(std::abs((1.0 + stepResidual(0.0 + 1e-12)) - 0.5) < 1e-9);
    CHECK(std::abs(stepResidual(-1e-12) - 0.5) < 1e-9);
    for (double x = 0.1; x < 2.0; x += 0.2) {
        const double after = 1.0 + stepResidual(x);
        const double before = 0.0 + stepResidual(-x);
        CHECK(std::abs(after + before - 1.0) < 1e-12);
    }
}

TEST_CASE("smoothed step is monotonic")
{
    double previous = 0.0;
    for (double x = -2.0; x < 2.0; x += 0.01) {
        const double smoothed = (x >= 0.0 ? 1.0 : 0.0) + stepResidual(x);
        CHECK(smoothed >= previous - 1e-12);
        previous = smoothed;
    }
    CHECK(std::abs(previous - 1.0) < 1e-2);
}

TEST_CASE("ramp residual is even, continuous, and peaks at the kink")
{
    for (double x = 0.05; x < 2.0; x += 0.1)
        CHECK(std::abs(rampResidual(x) - rampResidual(-x)) < 1e-12);
    CHECK(std::abs(rampResidual(0.0) - 7.0 / 30.0) < 1e-12);
    CHECK(std::abs(rampResidual(1.0 - 1e-9) - rampResidual(1.0 + 1e-9)) < 1e-8);
    CHECK(rampResidual(2.0) == 0.0);
}

TEST_CASE("ramp residual is the integral of the step residual")
{
    // d/dx (ramp residual) = step residual + step function, i.e. the smoothed step minus 1 for x >
    // 0.
    constexpr double kH = 1e-5;
    for (double x : {-1.7, -1.2, -0.6, -0.2, 0.3, 0.8, 1.4}) {
        const double derivative = (rampResidual(x + kH) - rampResidual(x - kH)) / (2.0 * kH);
        CHECK(std::abs(derivative - stepResidual(x)) < 1e-6);
    }
}
