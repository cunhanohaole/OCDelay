#include "TempoSync.h"

namespace TempoSync
{
    juce::StringArray getDivisionNames()
    {
        juce::StringArray names;

        for (const auto& division : divisions)
            names.add (division.name);

        return names;
    }

    double divisionToMs (int divisionIndex, double bpm)
    {
        jassert (bpm > 0.0);

        const auto index = (size_t) juce::jlimit (0, (int) divisions.size() - 1, divisionIndex);
        const auto msPerWholeNote = 4.0 * 60000.0 / bpm;
        return divisions[index].wholeNotes * msPerWholeNote;
    }
}
