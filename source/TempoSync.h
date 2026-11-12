#pragma once

#include <juce_core/juce_core.h>

// Note divisions for tempo-synced delay times, ordered from shortest to longest so that
// turning the knob always lengthens the delay.
namespace TempoSync
{
    struct Division
    {
        const char* name;
        double wholeNotes; // length as a fraction of a whole note (4 beats)
    };

    inline constexpr std::array<Division, 17> divisions { {
        { "1/64", 1.0 / 64.0 },
        { "1/32T", 1.0 / 48.0 },
        { "1/32", 1.0 / 32.0 },
        { "1/16T", 1.0 / 24.0 },
        { "1/32D", 3.0 / 64.0 },
        { "1/16", 1.0 / 16.0 },
        { "1/8T", 1.0 / 12.0 },
        { "1/16D", 3.0 / 32.0 },
        { "1/8", 1.0 / 8.0 },
        { "1/4T", 1.0 / 6.0 },
        { "1/8D", 3.0 / 16.0 },
        { "1/4", 1.0 / 4.0 },
        { "1/2T", 1.0 / 3.0 },
        { "1/4D", 3.0 / 8.0 },
        { "1/2", 1.0 / 2.0 },
        { "1/2D", 3.0 / 4.0 },
        { "1/1", 1.0 },
    } };

    inline constexpr int defaultDivisionIndex = 11; // 1/4: 500 ms at 120 BPM, matching the ms default
    inline constexpr double fallbackBpm = 120.0; // used when the host doesn't report a tempo

    juce::StringArray getDivisionNames();

    // Delay length in milliseconds for a division index at the given tempo
    double divisionToMs (int divisionIndex, double bpm);
}
