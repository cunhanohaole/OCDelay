#pragma once

#include <cmath>

// Maps a normalised speed-curve value (0 = bottom, 1 = top) to the time between repeat starts.
// The scale sits between two that were tried by ear: a log scale (equal ratios) reached the fast
// end far too early, and equal milliseconds only got fast at the very end. Stepping the square
// root of the interval evenly lands in between: a straight line builds steadily, then rushes.
namespace SpeedMapping
{
    // "Max" end rate: fast enough that the repeats blur into a pitched buzz (about 200 Hz)
    inline constexpr double kMaxSpeedIntervalMs = 5.0;

    // 1 = equal milliseconds; towards 0 = a log scale
    inline constexpr double kScaleExponent = 0.5;

    // interval = (start^p + (end^p − start^p) × v)^(1/p), p = kScaleExponent
    inline double intervalAt (double startInterval, double endInterval, float curveValue)
    {
        const auto start = std::pow (startInterval, kScaleExponent);
        const auto end = std::pow (endInterval, kScaleExponent);
        return std::pow (start + (end - start) * static_cast<double> (curveValue), 1.0 / kScaleExponent);
    }

    // Pitch Follow: a repeat plays faster in proportion to how much shorter its interval is
    inline double followRate (double startInterval, double interval)
    {
        return startInterval / interval;
    }
}
