#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <span>

using StereoFrame = std::array<float, 2>;

// One triggered tail: captures a slice, then starts a repeat of it every interval until the
// tail's length, each repeat played by its own head with its own pitch and fades.
// Owned and driven by RepeaterEngine; knows nothing about parameters, tempo or UI.
class RepeatVoice
{
public:
    // Overlapping repeats beyond this are cut as the next one starts
    static constexpr int kMaxHeads = 16;

    struct Settings
    {
        int sliceSamples = 1;
        int intervalSamples = 1; // time between repeat starts
        int lengthSamples = 1;
        bool overlap = false; // false: each repeat ends where the next begins
        int preRollSamples = 0; // samples already captured before the hit was detected
    };

    // Values that may change while the tail plays
    struct Context
    {
        std::span<const float> volumeTable;
        std::span<const float> pitchTable; // empty = no pitch change
        float pitchRangeSemitones = 12.0f;
        bool pitchBipolar = false;
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
        bool active = false;
    };

    void startHead (const Context& context);
    double rateAt (int voicePosition, const Context& context) const;
    int playedSamplesFor (int startPosition, double rate) const;
    StereoFrame renderHead (const Head& head, const Context& context) const;
    StereoFrame pan (float left, float right, int repeatIndex, const Context& context) const;
    float sharedGain (const Context& context) const;
    void advance();

    float readInterpolated (int channel, double position) const;

    juce::AudioBuffer<float> capture;
    std::array<Head, kMaxHeads> heads;
    int nextHeadSlot = 0;
    int nextRepeatStart = 0;

    Settings settings;
    int position = 0; // samples since the voice origin (detection minus pre-roll)
    int numChannels = 1;
    int maxEdgeFadeSamples = 1;
    int restartFadeSamples = 1;
    int restartFadeRemaining = 0;
    bool restartFading = false;
    bool active = false;
};
