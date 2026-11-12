#pragma once

#include "TriggerDetector.h"
#include <array>
#include <atomic>
#include <juce_dsp/juce_dsp.h>
#include <span>

// Repeater for single-shot sounds. A hit (TriggerDetector) captures a slice of the input,
// then plays it back to back until the latched length is reached, shaped by a volume curve.
// A pitch curve sets each repeat's playback rate (varispeed), held for the whole repeat.
// Two voices alternate so a retrigger can fade the old tail out while the new one captures.
// Handles 1 (mono) or 2 (stereo) channels. Knows nothing about parameters, tempo or UI:
// the caller converts slice and length to samples.
class RepeaterEngine
{
public:
    static constexpr double kPreRollSeconds = 0.005;
    static constexpr double kEdgeFadeSeconds = 0.003;
    static constexpr double kRestartFadeSeconds = 0.003;
    static constexpr double kParameterRampSeconds = 0.02;

    void prepare (double sampleRate, int maxBlockSize, int numChannels, double maxSliceSeconds);
    void reset();

    // Slice and length are latched on each hit; changes apply from the next hit.
    void setSliceSamples (int newSliceSamples);
    void setLengthSamples (int newLengthSamples);
    void setThresholdDb (float newThresholdDb);
    void setMix (float newMix);
    void setPingPong (bool shouldPingPong);
    void setPingPongWidth (float newWidth); // 0 = repeats centred, 1 = hard left/right

    // How pitch-curve values map to semitones; read as each repeat starts.
    void setPitchRange (float rangeSemitones, bool bipolar);

    // Both tables are sampled evenly from t = 0 to t = length inclusive.
    // volumeTable: wet gain. pitchTable: normalised pitch-curve values (empty = no pitch change).
    void process (juce::AudioBuffer<float>& buffer, std::span<const float> volumeTable, std::span<const float> pitchTable = {});

    // Position of the active tail as t / length in [0, 1), or -1 when idle. Safe from any thread.
    float getPlayheadPosition() const { return playheadPosition.load (std::memory_order_relaxed); }

    int getPreRollSamples() const { return preRollSamples; }
    int getEdgeFadeSamples() const { return maxEdgeFadeSamples; }

private:
    using Frame = std::array<float, 2>;

    struct Voice
    {
        juce::AudioBuffer<float> capture;
        int position = 0; // samples since the voice origin (detection minus pre-roll)
        int sliceSamples = 1;
        int lengthSamples = 1;
        int edgeFadeSamples = 1; // for the current repeat
        int playedSamples = 1; // samples of the current repeat that hold audio at its rate
        double rate = 1.0; // playback rate of the current repeat
        int restartFadeRemaining = 0;
        bool restartFading = false;
        bool active = false;

        bool isCapturing() const { return active && position < sliceSamples; }
        bool isRepeating() const { return active && position >= sliceSamples && position < lengthSamples; }
    };

    struct Panning
    {
        float amount; // 0 = plain stereo repeats, 1 = full ping-pong
        float width;
    };

    struct Tables
    {
        std::span<const float> volume;
        std::span<const float> pitch;
    };

    void processFrame (juce::AudioBuffer<float>& buffer, int index, const Tables& tables);
    Frame readFrame (const juce::AudioBuffer<float>& buffer, int index) const;
    void writeFrame (juce::AudioBuffer<float>& buffer, int index, const Frame& frame) const;

    void detectHit (const Frame& dry);
    void startNewVoice();
    void latchSettings (Voice& voice) const;
    void copyPreRollInto (Voice& voice, int numSamples) const;
    void pushPreRoll (const Frame& dry);

    Frame renderVoice (Voice& voice, const Frame& dry, const Panning& panning, const Tables& tables) const;
    void captureSample (Voice& voice, const Frame& dry) const;
    void startRepeat (Voice& voice, std::span<const float> pitchTable) const;
    void setRepeatRate (Voice& voice, double rate) const;
    Frame readRepeat (const Voice& voice, const Panning& panning) const;
    float wetGain (const Voice& voice, std::span<const float> volumeTable) const;
    float restartFadeGain (const Voice& voice) const;
    void advanceVoice (Voice& voice) const;
    void publishPlayhead();

    static float lookUp (std::span<const float> table, double normalisedTime);
    static float readInterpolated (const juce::AudioBuffer<float>& capture, int channel, double position, int sliceSamples);
    static float edgeFadeGain (int slicePosition, int playedSamples, int fadeSamples);
    static float endFadeGain (int samplesToEnd, int fadeSamples);

    TriggerDetector detector;
    std::array<Voice, 2> voices;
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
    int targetLengthSamples = 1;
    float pitchRangeSemitones = 12.0f;
    bool pitchBipolar = false;
};
