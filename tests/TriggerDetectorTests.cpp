#include <catch2/catch_test_macros.hpp>
#include <dsp/TriggerDetector.h>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr float kLoud = 0.5f;  // about -6 dB
    constexpr float kQuiet = 0.01f; // -40 dB, below the -30 dB threshold

    TriggerDetector makeDetector()
    {
        TriggerDetector detector;
        detector.setThresholdDb (-30.0f);
        detector.prepare (kSampleRate);
        return detector;
    }

    int countFires (TriggerDetector& detector, float level, int numSamples)
    {
        int fires = 0;
        for (int i = 0; i < numSamples; ++i)
            fires += detector.processSample (level) ? 1 : 0;
        return fires;
    }
}

TEST_CASE ("Detector fires on the first sample above threshold", "[TriggerDetector]")
{
    auto detector = makeDetector();

    CHECK_FALSE (detector.processSample (kQuiet));
    CHECK (detector.processSample (kLoud));
}

TEST_CASE ("Detector never fires below threshold", "[TriggerDetector]")
{
    auto detector = makeDetector();

    CHECK (countFires (detector, kQuiet, 48000) == 0);
}

TEST_CASE ("A sustained loud signal fires only once", "[TriggerDetector]")
{
    auto detector = makeDetector();

    CHECK (countFires (detector, kLoud, 48000) == 1);
}

TEST_CASE ("Detector re-arms only after 50 ms below threshold", "[TriggerDetector]")
{
    auto detector = makeDetector();
    const auto rearmSamples = detector.getRearmSamples();
    REQUIRE (rearmSamples == 2400);

    REQUIRE (detector.processSample (kLoud));

    SECTION ("one sample short of the re-arm time is not enough")
    {
        countFires (detector, kQuiet, rearmSamples - 1);
        CHECK_FALSE (detector.processSample (kLoud));
    }

    SECTION ("a loud blip restarts the quiet count")
    {
        countFires (detector, kQuiet, rearmSamples - 1);
        CHECK_FALSE (detector.processSample (kLoud));
        countFires (detector, kQuiet, rearmSamples - 1);
        CHECK_FALSE (detector.processSample (kLoud));
    }

    SECTION ("the full re-arm time arms it again")
    {
        countFires (detector, kQuiet, rearmSamples);
        CHECK (detector.processSample (kLoud));
    }
}

TEST_CASE ("Reset re-arms the detector", "[TriggerDetector]")
{
    auto detector = makeDetector();
    REQUIRE (detector.processSample (kLoud));

    detector.reset();

    CHECK (detector.processSample (kLoud));
}
