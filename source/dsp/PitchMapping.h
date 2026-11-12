#pragma once

#include <cmath>

// Maps a normalised pitch-curve value (0-1) to semitones and back.
//   unipolar: 0 = no change, 1 = +range
//   bipolar:  0.5 = no change, 1 = +range, 0 = -range
namespace PitchMapping
{
    inline float toSemitones (float curveValue, float rangeSemitones, bool bipolar)
    {
        return bipolar ? (2.0f * curveValue - 1.0f) * rangeSemitones
                       : curveValue * rangeSemitones;
    }

    inline float toCurveValue (float semitones, float rangeSemitones, bool bipolar)
    {
        const auto fraction = semitones / rangeSemitones;
        return bipolar ? 0.5f * (fraction + 1.0f) : fraction;
    }

    inline double semitonesToRate (float semitones)
    {
        return std::exp2 (static_cast<double> (semitones) / 12.0);
    }
}
