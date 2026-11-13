#include "RepeaterEngine.h"

namespace
{
    // Rounds rather than ceils: 0.005 * 48000 isn't exactly 240 in floating point.
    int secondsToSamples (double seconds, double sampleRate)
    {
        return juce::jmax (1, juce::roundToInt (seconds * sampleRate));
    }
}

void RepeaterEngine::prepare (double newSampleRate, int maxBlockSize, int numChannels, double maxSliceSeconds)
{
    jassert (newSampleRate > 0.0 && maxBlockSize > 0 && maxSliceSeconds > 0.0);
    jassert (numChannels == 1 || numChannels == 2);

    sampleRate = newSampleRate;
    numPreparedChannels = juce::jlimit (1, 2, numChannels);
    maxBlockSamples = maxBlockSize;
    maxSliceSamples = secondsToSamples (maxSliceSeconds, sampleRate);
    preRollSamples = secondsToSamples (kPreRollSeconds, sampleRate);
    maxEdgeFadeSamples = secondsToSamples (kEdgeFadeSeconds, sampleRate);
    restartFadeSamples = secondsToSamples (kRestartFadeSeconds, sampleRate);

    preRoll.setSize (numPreparedChannels, preRollSamples);
    for (auto& voice : voices)
        voice.prepare (numPreparedChannels, maxSliceSamples, maxEdgeFadeSamples, restartFadeSamples);

    detector.prepare (sampleRate);
    mix.reset (sampleRate, kParameterRampSeconds);
    pingPongAmount.reset (sampleRate, kParameterRampSeconds);
    pingPongWidth.reset (sampleRate, kParameterRampSeconds);

    reset();
}

void RepeaterEngine::reset()
{
    for (auto& voice : voices)
        voice.stop();

    activeVoiceIndex = 0;
    preRoll.clear();
    preRollWriteIndex = 0;
    detector.reset();

    mix.setCurrentAndTargetValue (mix.getTargetValue());
    pingPongAmount.setCurrentAndTargetValue (pingPongAmount.getTargetValue());
    pingPongWidth.setCurrentAndTargetValue (pingPongWidth.getTargetValue());
    playheadPosition.store (-1.0f, std::memory_order_relaxed);
}

void RepeaterEngine::stop()
{
    for (auto& voice : voices)
        voice.beginRestartFade();
}

void RepeaterEngine::setSliceSamples (int newSliceSamples)
{
    targetSliceSamples = juce::jmax (1, newSliceSamples);
}

void RepeaterEngine::setIntervalSamples (int newIntervalSamples)
{
    targetIntervalSamples = juce::jmax (0, newIntervalSamples);
}

void RepeaterEngine::setOverlap (bool shouldOverlap)
{
    targetOverlap = shouldOverlap;
}

void RepeaterEngine::setLengthSamples (int newLengthSamples)
{
    targetLengthSamples = juce::jmax (1, newLengthSamples);
}

void RepeaterEngine::setThresholdDb (float newThresholdDb)
{
    detector.setThresholdDb (newThresholdDb);
}

void RepeaterEngine::setMix (float newMix)
{
    mix.setTargetValue (juce::jlimit (0.0f, 1.0f, newMix));
}

void RepeaterEngine::setPingPong (bool shouldPingPong)
{
    pingPongAmount.setTargetValue (shouldPingPong ? 1.0f : 0.0f);
}

void RepeaterEngine::setPingPongWidth (float newWidth)
{
    pingPongWidth.setTargetValue (juce::jlimit (0.0f, 1.0f, newWidth));
}

void RepeaterEngine::setEndIntervalSamples (double newEndIntervalSamples)
{
    endIntervalSamples = juce::jmax (0.0, newEndIntervalSamples);
}

void RepeaterEngine::setPitchFollow (bool shouldFollow)
{
    pitchFollow = shouldFollow;
}

void RepeaterEngine::process (juce::AudioBuffer<float>& buffer, std::span<const float> volumeTable, std::span<const float> speedTable)
{
    jassert (sampleRate > 0.0); // prepare() must be called first
    jassert (buffer.getNumSamples() <= maxBlockSamples);
    jassert (buffer.getNumChannels() >= numPreparedChannels);
    jassert (! volumeTable.empty());

    RepeatVoice::Context context;
    context.volumeTable = volumeTable;
    context.speedTable = speedTable;
    context.endIntervalSamples = endIntervalSamples;
    context.pitchFollow = pitchFollow;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        processFrame (buffer, i, context);

    publishPlayhead();
}

void RepeaterEngine::processFrame (juce::AudioBuffer<float>& buffer, int index, RepeatVoice::Context& context)
{
    const auto dry = readFrame (buffer, index);
    detectHit (dry);

    context.pingPongAmount = pingPongAmount.getNextValue();
    context.pingPongWidth = pingPongWidth.getNextValue();

    StereoFrame wet {};
    for (auto& voice : voices)
    {
        const auto voiceWet = voice.render (dry, context);
        wet[0] += voiceWet[0];
        wet[1] += voiceWet[1];
    }

    pushPreRoll (dry);

    const auto wetLevel = mix.getNextValue();
    const auto dryLevel = 1.0f - wetLevel;
    writeFrame (buffer, index, { dry[0] * dryLevel + wet[0] * wetLevel, dry[1] * dryLevel + wet[1] * wetLevel });
}

StereoFrame RepeaterEngine::readFrame (const juce::AudioBuffer<float>& buffer, int index) const
{
    StereoFrame frame {};
    for (int channel = 0; channel < numPreparedChannels; ++channel)
        frame[static_cast<size_t> (channel)] = buffer.getSample (channel, index);
    return frame;
}

void RepeaterEngine::writeFrame (juce::AudioBuffer<float>& buffer, int index, const StereoFrame& frame) const
{
    for (int channel = 0; channel < numPreparedChannels; ++channel)
        buffer.setSample (channel, index, frame[static_cast<size_t> (channel)]);
}

void RepeaterEngine::detectHit (const StereoFrame& dry)
{
    const auto peak = juce::jmax (std::abs (dry[0]), std::abs (dry[1]));

    // The detector always runs so its re-arm timing stays honest, but hits while the
    // active voice is still capturing are part of the slice.
    if (detector.processSample (peak) && ! voices[static_cast<size_t> (activeVoiceIndex)].isCapturing())
        startNewVoice();
}

void RepeaterEngine::startNewVoice()
{
    voices[static_cast<size_t> (activeVoiceIndex)].beginRestartFade();

    activeVoiceIndex = 1 - activeVoiceIndex;
    auto& newVoice = voices[static_cast<size_t> (activeVoiceIndex)];
    const auto settings = makeVoiceSettings();
    newVoice.start (settings);
    copyPreRollInto (newVoice, settings.preRollSamples);
}

RepeatVoice::Settings RepeaterEngine::makeVoiceSettings() const
{
    RepeatVoice::Settings settings;
    settings.lengthSamples = targetLengthSamples;
    settings.sliceSamples = juce::jlimit (1, juce::jmin (maxSliceSamples, settings.lengthSamples), targetSliceSamples);

    const auto interval = targetIntervalSamples > 0 ? targetIntervalSamples : settings.sliceSamples;
    settings.intervalSamples = juce::jlimit (1, settings.lengthSamples, interval);
    // Free Rate off (interval 0) repeats back to back: a sped-up repeat is cut at the next start
    settings.overlap = targetIntervalSamples > 0 && targetOverlap;

    // Pre-roll is limited to half the slice so the hit itself always lands inside it.
    settings.preRollSamples = juce::jmin (preRollSamples, settings.sliceSamples / 2);
    return settings;
}

void RepeaterEngine::copyPreRollInto (RepeatVoice& voice, int numSamples) const
{
    // preRollWriteIndex points at the oldest sample, so the newest numSamples end just before it.
    const auto firstIndex = preRollWriteIndex - numSamples + preRollSamples;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto readIndex = (firstIndex + i) % preRollSamples;
        StereoFrame frame {};
        for (int channel = 0; channel < numPreparedChannels; ++channel)
            frame[static_cast<size_t> (channel)] = preRoll.getSample (channel, readIndex);
        voice.writeCapture (i, frame);
    }
}

void RepeaterEngine::pushPreRoll (const StereoFrame& dry)
{
    for (int channel = 0; channel < numPreparedChannels; ++channel)
        preRoll.setSample (channel, preRollWriteIndex, dry[static_cast<size_t> (channel)]);

    preRollWriteIndex = (preRollWriteIndex + 1) % preRollSamples;
}

void RepeaterEngine::publishPlayhead()
{
    playheadPosition.store (voices[static_cast<size_t> (activeVoiceIndex)].getPlayheadPosition(), std::memory_order_relaxed);
}
