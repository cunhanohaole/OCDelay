#pragma once

#include "RepeatVoice.h"
#include "TriggerDetector.h"
#include <array>
#include <atomic>
#include <juce_dsp/juce_dsp.h>
#include <span>

// Repeater for single-shot sounds. A hit (TriggerDetector) captures a slice of the input, then
// repeats it until the latched length is reached, shaped by a volume curve. A speed curve moves
// the time between repeats from the start interval towards an end interval; with Pitch Follow
// each repeat also plays faster or slower in proportion (varispeed), held for the whole repeat.
// Two voices alternate so a retrigger can fade the old tail out while the new one captures.
// Handles 1 (mono) or 2 (stereo) channels. Knows nothing about parameters, tempo or UI:
// the caller converts slice, interval and length to samples.
class RepeaterEngine
{
public:
    static constexpr double kPreRollSeconds = 0.005;
    static constexpr double kEdgeFadeSeconds = 0.003;
    static constexpr double kRestartFadeSeconds = 0.003;
    static constexpr double kParameterRampSeconds = 0.02;

    void prepare (double sampleRate, int maxBlockSize, int numChannels, double maxSliceSeconds);
    void reset();

    // Fades every repeat out over the restart fade (3 ms). The next hit starts a new tail as usual.
    void stop();

    // Latched on each hit; changes apply from the next hit.
    void setSliceSamples (int newSliceSamples);
    void setIntervalSamples (int newIntervalSamples); // 0 = repeats back to back (every slice)
    void setOverlap (bool shouldOverlap);
    void setLengthSamples (int newLengthSamples);

    void setThresholdDb (float newThresholdDb);
    void setMix (float newMix);
    void setPingPong (bool shouldPingPong);
    void setPingPongWidth (float newWidth); // 0 = repeats centred, 1 = hard left/right

    // Read as each repeat starts. End interval = the interval at the top of the speed curve
    // (0 = the start interval, so the curve does nothing).
    void setEndIntervalSamples (double newEndIntervalSamples);
    void setPitchFollow (bool shouldFollow);

    // Both tables are sampled evenly from t = 0 to t = length inclusive.
    // volumeTable: wet gain. speedTable: normalised speed-curve values (empty = no speed change).
    void process (juce::AudioBuffer<float>& buffer, std::span<const float> volumeTable, std::span<const float> speedTable = {});

    // Position of the active tail as t / length in [0, 1), or -1 when idle. Safe from any thread.
    float getPlayheadPosition() const { return playheadPosition.load (std::memory_order_relaxed); }

    int getPreRollSamples() const { return preRollSamples; }
    int getEdgeFadeSamples() const { return maxEdgeFadeSamples; }

private:
    void processFrame (juce::AudioBuffer<float>& buffer, int index, RepeatVoice::Context& context);
    StereoFrame readFrame (const juce::AudioBuffer<float>& buffer, int index) const;
    void writeFrame (juce::AudioBuffer<float>& buffer, int index, const StereoFrame& frame) const;

    void detectHit (const StereoFrame& dry);
    void startNewVoice();
    RepeatVoice::Settings makeVoiceSettings() const;
    void copyPreRollInto (RepeatVoice& voice, int numSamples) const;
    void pushPreRoll (const StereoFrame& dry);
    void publishPlayhead();

    TriggerDetector detector;
    std::array<RepeatVoice, 2> voices;
    int activeVoiceIndex = 0;

    juce::AudioBuffer<float> preRoll;
    int preRollWriteIndex = 0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pingPongAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pingPongWidth { 1.0f };

    std::atomic<float> playheadPosition { -1.0f };

    double sampleRate = 0.0;
    int numPreparedChannels = 0;
    int maxBlockSamples = 0;
    int maxSliceSamples = 1;
    int preRollSamples = 1;
    int maxEdgeFadeSamples = 1;
    int restartFadeSamples = 1;

    int targetSliceSamples = 1;
    int targetIntervalSamples = 0;
    int targetLengthSamples = 1;
    bool targetOverlap = false;
    double endIntervalSamples = 0.0;
    bool pitchFollow = false;
};
