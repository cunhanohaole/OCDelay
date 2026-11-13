#pragma once

#include <cmath>

// Maps a normalised speed-curve value (0 = bottom, 1 = top) to the time between repeat starts.
// The scale is logarithmic, so a straight line sounds like an even acceleration.
namespace SpeedMapping
{
    // "Max" end rate: fast enough that the repeats blur into a pitched buzz (about 200 Hz)
    inline constexpr double kMaxSpeedIntervalMs = 5.0;

    // interval = start × (end ÷ start)^v
    inline double intervalAt (double startInterval, double endInterval, float curveValue)
    {
        return startInterval * std::pow (endInterval / startInterval, static_cast<double> (curveValue));
    }

    // Pitch Follow: a repeat plays faster in proportion to how much shorter its interval is
    inline double followRate (double startInterval, double interval)
    {
        return startInterval / interval;
    }
}
