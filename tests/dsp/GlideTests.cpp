#include "dsp/Glide.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace polylogue::dsp;

namespace {

constexpr double kFs = 48000.0;

Glide make(float seconds, float from, float to)
{
    Glide glide;
    glide.prepare(kFs);
    glide.setTime(seconds);
    glide.jumpTo(from);
    glide.glideTo(to);
    return glide;
}

}  // namespace

TEST_CASE("glide arrives within five percent at its time")
{
    Glide glide = make(0.2f, 48.0f, 60.0f);
    float value = 0.0f;
    for (int i = 0; i < static_cast<int>(0.2 * kFs); ++i)
        value = glide.next();
    CHECK(std::abs(value - 60.0f) < 0.06f * 12.0f);
    CHECK(value < 60.0f);
}

TEST_CASE("glide moves monotonically and settles exactly")
{
    Glide glide = make(0.05f, 72.0f, 40.0f);
    float previous = 72.0f;
    for (int i = 0; i < 48000; ++i) {
        const float value = glide.next();
        CHECK(value <= previous);
        previous = value;
    }
    CHECK(previous == 40.0f);
}

TEST_CASE("zero time jumps at once")
{
    Glide glide = make(0.0f, 40.0f, 65.0f);
    CHECK(glide.next() == 65.0f);
}

TEST_CASE("jumpTo cancels a glide in progress")
{
    Glide glide = make(1.0f, 40.0f, 65.0f);
    glide.next();
    glide.jumpTo(50.0f);
    CHECK(glide.next() == 50.0f);
}
