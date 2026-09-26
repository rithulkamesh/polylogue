#include "dsp/Tuning.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace polylogue::dsp;
using Catch::Approx;

TEST_CASE("midiToHz follows equal temperament")
{
    CHECK(midiToHz(69.0) == Approx(440.0));
    CHECK(midiToHz(60.0) == Approx(261.6255653).epsilon(1e-9));
    CHECK(midiToHz(81.0) == Approx(880.0));
    CHECK(midiToHz(57.0) == Approx(220.0));
}

TEST_CASE("dbToGain converts decibels")
{
    CHECK(dbToGain(0.0f) == Approx(1.0f));
    CHECK(dbToGain(-6.0206f) == Approx(0.5f).epsilon(1e-4));
    CHECK(dbToGain(20.0f) == Approx(10.0f));
}
