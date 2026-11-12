#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Detects hits: the input level rising above a threshold. After firing it disarms and only
// re-arms once the level has stayed below the threshold for kRearmSeconds, so a single
// sound (or its own decay wobbling around the threshold) can't trigger itself twice.
class TriggerDetector
{
public:
    static constexpr double kRearmSeconds = 0.05;
    static constexpr float kDefaultThresholdDb = -30.0f;

    void prepare (double sampleRate);
    void setThresholdDb (float newThresholdDb);
    void reset();

    // Feed one sample's absolute peak level (max over channels). Returns true on the
    // sample the hit fires.
    bool processSample (float peakAbs);

    int getRearmSamples() const { return rearmSamples; }

private:
    void countQuietSample();

    float thresholdLinear = juce::Decibels::decibelsToGain (kDefaultThresholdDb);
    int rearmSamples = 1;
    int quietSamples = 0;
    bool armed = true;
};
