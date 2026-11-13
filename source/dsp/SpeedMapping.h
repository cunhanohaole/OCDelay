#pragma once

// Maps a normalised speed-curve value (0 = bottom, 1 = top) to the time between repeat starts.
// Equal steps up the curve shorten the gap by equal milliseconds, so a straight line stays slow
// for most of the tail and rushes at the end, like a bouncing ball or a classic riser.
// (A log scale reached the fast end far too early: it halves the gap at a constant rate.)
namespace SpeedMapping
{
    // "Max" end rate: fast enough that the repeats blur into a pitched buzz (about 200 Hz)
    inline constexpr double kMaxSpeedIntervalMs = 5.0;

    // interval = start + (end − start) × v
    inline double intervalAt (double startInterval, double endInterval, float curveValue)
    {
        return startInterval + (endInterval - startInterval) * static_cast<double> (curveValue);
    }

    // Pitch Follow: a repeat plays faster in proportion to how much shorter its interval is
    inline double followRate (double startInterval, double interval)
    {
        return startInterval / interval;
    }
}
