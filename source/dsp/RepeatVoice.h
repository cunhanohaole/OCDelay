#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <span>

using StereoFrame = std::array<float, 2>;

// One triggered tail: captures a slice, then starts repeats of it until the tail's length.
// The speed curve sets each repeat's interval (from the start interval towards the end
// interval); each repeat is played by its own head with its own playback rate and fades.
// Owned and driven by RepeaterEngine; knows nothing about parameters, tempo or UI.
class RepeatVoice
{
public:
    // Overlapping repeats beyond this are cut as the next one starts
    static constexpr int kMaxHeads = 16;

    struct Settings
    {
        int sliceSamples = 1;
        int intervalSamples = 1; // time between repeat starts at the bottom of the speed curve
        int lengthSamples = 1;
        bool overlap = false; // false: each repeat ends where the next begins
        int preRollSamples = 0; // samples already captured before the hit was detected
    };

    // Values that may change while the tail plays
    struct Context
    {
        std::span<const float> volumeTable;
        std::span<const float> speedTable; // empty = no speed change
        double endIntervalSamples = 0.0; // interval at the top of the speed curve; 0 = the start interval
        bool pitchFollow = false; // repeats play faster or slower in proportion to their interval
        float pingPongAmount = 0.0f;
        float pingPongWidth = 1.0f;
    };

    void prepare (int numChannels, int maxSliceSamples, int maxEdgeFadeSamples, int restartFadeSamples);
    void start (const Settings& settings);
    void stop() { active = false; }
    void beginRestartFade();

    void writeCapture (int index, const StereoFrame& frame);
    StereoFrame render (const StereoFrame& dry, const Context& context);

    bool isActive() const { return active; }
    bool isCapturing() const { return active && position < settings.sliceSamples; }
    float getPlayheadPosition() const;

private:
    struct Head
    {
        int startPosition = 0;
        int repeatIndex = 0;
        int playedSamples = 0; // output samples that hold audio
        int edgeFadeSamples = 1;
        double rate = 1.0;
        double readOffset = 0.0; // slice position the repeat starts reading from
        bool active = false;
    };

    void scheduleFirstRepeat (const Context& context);
    void scheduleNextRepeat (int fromPosition, const Context& context);
    void startHead (const Context& context);
    double intervalAt (int voicePosition, const Context& context) const;
    int stepFor (double interval) const;
    int edgeFadeFor (int playedSamples) const;
    double readOffsetFor (double sliceSamplesPerRepeat, double fadeInSliceSamples) const;
    int playedSamplesFor (int startPosition, double rate, double readOffset, int step) const;
    void trimHeadReusedAt (int nextStart);
    StereoFrame renderHead (const Head& head, const Context& context) const;
    StereoFrame pan (float left, float right, int repeatIndex, const Context& context) const;
    float sharedGain (const Context& context) const;
    void advance();

    float readInterpolated (int channel, double position) const;

    juce::AudioBuffer<float> capture;
    std::array<Head, kMaxHeads> heads;
    int nextHeadSlot = 0;
    int nextRepeatStart = -1; // -1 until the first render schedules it
    int nextRepeatIndex = 1;

    Settings settings;
    int position = 0; // samples since the voice origin (detection minus pre-roll)
    int numChannels = 1;
    int maxEdgeFadeSamples = 1;
    int restartFadeSamples = 1;
    int restartFadeRemaining = 0;
    bool restartFading = false;
    bool active = false;
};
