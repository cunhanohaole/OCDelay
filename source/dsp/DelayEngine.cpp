#include "DelayEngine.h"

void DelayEngine::prepare (double newSampleRate, int maxBlockSize, int numChannels, double maxDelaySeconds)
{
    jassert (newSampleRate > 0.0 && maxBlockSize > 0 && maxDelaySeconds > 0.0);
    jassert (numChannels == 1 || numChannels == 2);

    sampleRate = newSampleRate;
    numPreparedChannels = juce::jlimit (1, 2, numChannels);
    maxDelaySamples = (float) std::ceil (maxDelaySeconds * sampleRate);

    delayLine.setMaximumDelayInSamples ((int) maxDelaySamples + kDelayLineHeadroomSamples);
    delayLine.prepare ({ sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) numPreparedChannels });

    delaySamples.reset (sampleRate, kDelayGlideSeconds);
    feedback.reset (sampleRate, kParameterRampSeconds);
    mix.reset (sampleRate, kParameterRampSeconds);
    pingPongAmount.reset (sampleRate, kParameterRampSeconds);

    updateDelayTarget();
    reset();
}

void DelayEngine::reset()
{
    delayLine.reset();

    delaySamples.setCurrentAndTargetValue (delaySamples.getTargetValue());
    feedback.setCurrentAndTargetValue (feedback.getTargetValue());
    mix.setCurrentAndTargetValue (mix.getTargetValue());
    pingPongAmount.setCurrentAndTargetValue (pingPongAmount.getTargetValue());
}

void DelayEngine::setDelayTimeMs (float newDelayTimeMs)
{
    targetDelayTimeMs = newDelayTimeMs;
    updateDelayTarget();
}

void DelayEngine::setFeedback (float newFeedback)
{
    feedback.setTargetValue (juce::jlimit (0.0f, kMaxFeedback, newFeedback));
}

void DelayEngine::setMix (float newMix)
{
    mix.setTargetValue (juce::jlimit (0.0f, 1.0f, newMix));
}

void DelayEngine::setPingPong (bool shouldPingPong)
{
    pingPongAmount.setTargetValue (shouldPingPong ? 1.0f : 0.0f);
}

void DelayEngine::process (juce::AudioBuffer<float>& buffer)
{
    jassert (sampleRate > 0.0); // prepare() must be called first

    const auto numChannels = juce::jmin (buffer.getNumChannels(), numPreparedChannels);
    const auto numSamples = buffer.getNumSamples();

    if (numChannels == 2)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
            processStereoSample (left[i], right[i], nextSampleParameters());
    }
    else if (numChannels == 1)
    {
        auto* samples = buffer.getWritePointer (0);

        for (int i = 0; i < numSamples; ++i)
            processMonoSample (samples[i], nextSampleParameters());
    }
}

DelayEngine::SampleParameters DelayEngine::nextSampleParameters()
{
    return { delaySamples.getNextValue(),
        feedback.getNextValue(),
        mix.getNextValue(),
        pingPongAmount.getNextValue() };
}

// Ping-pong crossfades the routing so toggling it never clicks:
//   off: each channel feeds back into itself
//   on:  mono input enters the left line, and each line feeds the other
void DelayEngine::processStereoSample (float& left, float& right, const SampleParameters& params)
{
    const auto inLeft = left;
    const auto inRight = right;
    const auto p = params.pingPong;

    const auto outLeft = delayLine.popSample (0, params.delaySamples);
    const auto outRight = delayLine.popSample (1, params.delaySamples);

    const auto monoIn = 0.5f * (inLeft + inRight);
    const auto writeLeft = juce::jmap (p, inLeft, monoIn) + params.feedback * juce::jmap (p, outLeft, outRight);
    const auto writeRight = juce::jmap (p, inRight, 0.0f) + params.feedback * juce::jmap (p, outRight, outLeft);

    delayLine.pushSample (0, writeLeft);
    delayLine.pushSample (1, writeRight);

    left = mixDryWet (inLeft, outLeft, params.mix);
    right = mixDryWet (inRight, outRight, params.mix);
}

void DelayEngine::processMonoSample (float& sample, const SampleParameters& params)
{
    const auto in = sample;
    const auto out = delayLine.popSample (0, params.delaySamples);

    delayLine.pushSample (0, in + params.feedback * out);

    sample = mixDryWet (in, out, params.mix);
}

void DelayEngine::updateDelayTarget()
{
    if (sampleRate <= 0.0)
        return; // applied in prepare()

    const auto samples = (float) (targetDelayTimeMs * sampleRate / 1000.0);
    delaySamples.setTargetValue (juce::jlimit (kMinDelaySamples, maxDelaySamples, samples));
}

float DelayEngine::mixDryWet (float dry, float wet, float mixAmount)
{
    return dry * (1.0f - mixAmount) + wet * mixAmount;
}
