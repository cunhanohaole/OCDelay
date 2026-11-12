#include "TriggerDetector.h"

void TriggerDetector::prepare (double sampleRate)
{
    jassert (sampleRate > 0.0);
    rearmSamples = juce::jmax (1, juce::roundToInt (kRearmSeconds * sampleRate));
    reset();
}

void TriggerDetector::setThresholdDb (float newThresholdDb)
{
    thresholdLinear = juce::Decibels::decibelsToGain (newThresholdDb);
}

void TriggerDetector::reset()
{
    quietSamples = 0;
    armed = true;
}

bool TriggerDetector::processSample (float peakAbs)
{
    if (peakAbs <= thresholdLinear)
    {
        countQuietSample();
        return false;
    }

    quietSamples = 0;

    if (! armed)
        return false;

    armed = false;
    return true;
}

void TriggerDetector::countQuietSample()
{
    if (armed)
        return;

    if (++quietSamples >= rearmSamples)
        armed = true;
}
