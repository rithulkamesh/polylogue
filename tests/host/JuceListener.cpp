// Keeps JUCE's message manager alive for exactly the duration of the test run, so shutdown is
// ordered and timers and parameter notifications work.
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <juce_events/juce_events.h>

#include <memory>

namespace {

class JuceListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(const Catch::TestRunInfo&) override
    {
        initialiser_ = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
    }

    void testRunEnded(const Catch::TestRunStats&) override { initialiser_.reset(); }

private:
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> initialiser_;
};

}  // namespace

CATCH_REGISTER_LISTENER(JuceListener)
