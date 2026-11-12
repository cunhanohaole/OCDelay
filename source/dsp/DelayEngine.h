#pragma once

#include <juce_dsp/juce_dsp.h>

// Clean digital delay with feedback, dry/wet mix and optional ping-pong.
// Handles 1 (mono) or 2 (stereo) channels. Knows nothing about parameters or UI.
class DelayEngine
{
public:
    // How long a delay-time change takes to reach its target. The read position glides,
    // which pitch-shifts the repeats tape-style instead of jumping (and clicking).
    static constexpr double kDelayGlideSeconds = 0.1;
    static constexpr double kParameterRampSeconds = 0.02;
    static constexpr float kMaxFeedback = 0.95f;

    void prepare (double sampleRate, int maxBlockSize, int numChannels, double maxDelaySeconds);
    void reset();

    void setDelayTimeMs (float newDelayTimeMs);

    // Feedback is the per-repeat gain. It is the single gain input on purpose: a future
    // tail-length / curve system will replace it as the source of per-repeat gain.
    void setFeedback (float newFeedback);
    void setMix (float newMix);
    void setPingPong (bool shouldPingPong);

    void process (juce::AudioBuffer<float>& buffer);

private:
    struct SampleParameters
    {
        float delaySamples;
        float feedback;
        float mix;
        float pingPong;
    };

    SampleParameters nextSampleParameters();
    void processStereoSample (float& left, float& right, const SampleParameters& params);
    void processMonoSample (float& sample, const SampleParameters& params);
    void updateDelayTarget();

    static float mixDryWet (float dry, float wet, float mix);

    // Lagrange 3rd needs one valid sample on each side of the read position, and we read
    // before writing, so the delay can never be shorter than 2 samples.
    static constexpr float kMinDelaySamples = 2.0f;
    static constexpr int kDelayLineHeadroomSamples = 4;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySamples;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedback;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pingPongAmount;

    double sampleRate = 0.0;
    int numPreparedChannels = 0;
    float maxDelaySamples = 0.0f;
    float targetDelayTimeMs = 500.0f;
};
