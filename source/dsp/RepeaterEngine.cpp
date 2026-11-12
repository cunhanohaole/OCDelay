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
        voice.capture.setSize (numPreparedChannels, maxSliceSamples);

    detector.prepare (sampleRate);
    mix.reset (sampleRate, kParameterRampSeconds);
    pingPongAmount.reset (sampleRate, kParameterRampSeconds);

    reset();
}

void RepeaterEngine::reset()
{
    for (auto& voice : voices)
        voice.active = false;

    activeVoiceIndex = 0;
    preRoll.clear();
    preRollWriteIndex = 0;
    detector.reset();

    mix.setCurrentAndTargetValue (mix.getTargetValue());
    pingPongAmount.setCurrentAndTargetValue (pingPongAmount.getTargetValue());
    playheadPosition.store (-1.0f, std::memory_order_relaxed);
}

void RepeaterEngine::setSliceSamples (int newSliceSamples)
{
    targetSliceSamples = juce::jmax (1, newSliceSamples);
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

void RepeaterEngine::process (juce::AudioBuffer<float>& buffer, std::span<const float> curveTable)
{
    jassert (sampleRate > 0.0); // prepare() must be called first
    jassert (buffer.getNumSamples() <= maxBlockSamples);
    jassert (buffer.getNumChannels() >= numPreparedChannels);
    jassert (! curveTable.empty());

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        processFrame (buffer, i, curveTable);

    publishPlayhead();
}

void RepeaterEngine::processFrame (juce::AudioBuffer<float>& buffer, int index, std::span<const float> curveTable)
{
    const auto dry = readFrame (buffer, index);
    detectHit (dry);

    const auto pingPong = pingPongAmount.getNextValue();
    Frame wet {};

    for (auto& voice : voices)
    {
        const auto voiceWet = renderVoice (voice, dry, pingPong, curveTable);
        wet[0] += voiceWet[0];
        wet[1] += voiceWet[1];
    }

    pushPreRoll (dry);

    const auto wetLevel = mix.getNextValue();
    const auto dryLevel = 1.0f - wetLevel;
    writeFrame (buffer, index, { dry[0] * dryLevel + wet[0] * wetLevel, dry[1] * dryLevel + wet[1] * wetLevel });
}

RepeaterEngine::Frame RepeaterEngine::readFrame (const juce::AudioBuffer<float>& buffer, int index) const
{
    Frame frame {};
    for (int channel = 0; channel < numPreparedChannels; ++channel)
        frame[static_cast<size_t> (channel)] = buffer.getSample (channel, index);
    return frame;
}

void RepeaterEngine::writeFrame (juce::AudioBuffer<float>& buffer, int index, const Frame& frame) const
{
    for (int channel = 0; channel < numPreparedChannels; ++channel)
        buffer.setSample (channel, index, frame[static_cast<size_t> (channel)]);
}

void RepeaterEngine::detectHit (const Frame& dry)
{
    const auto peak = juce::jmax (std::abs (dry[0]), std::abs (dry[1]));

    // The detector always runs so its re-arm timing stays honest, but hits while the
    // active voice is still capturing are part of the slice.
    if (detector.processSample (peak) && ! voices[static_cast<size_t> (activeVoiceIndex)].isCapturing())
        startNewVoice();
}

void RepeaterEngine::startNewVoice()
{
    auto& oldVoice = voices[static_cast<size_t> (activeVoiceIndex)];
    if (oldVoice.active && ! oldVoice.restartFading)
    {
        oldVoice.restartFading = true;
        oldVoice.restartFadeRemaining = restartFadeSamples;
    }

    activeVoiceIndex = 1 - activeVoiceIndex;
    auto& newVoice = voices[static_cast<size_t> (activeVoiceIndex)];
    latchSettings (newVoice);
    copyPreRollInto (newVoice, newVoice.position);
}

void RepeaterEngine::latchSettings (Voice& voice) const
{
    voice.lengthSamples = targetLengthSamples;
    voice.sliceSamples = juce::jlimit (1, juce::jmin (maxSliceSamples, voice.lengthSamples), targetSliceSamples);
    voice.edgeFadeSamples = juce::jlimit (1, maxEdgeFadeSamples, voice.sliceSamples / 2);

    // Pre-roll is limited to half the slice so the hit itself always lands inside it.
    voice.position = juce::jmin (preRollSamples, voice.sliceSamples / 2);
    voice.restartFading = false;
    voice.restartFadeRemaining = 0;
    voice.active = true;
}

void RepeaterEngine::copyPreRollInto (Voice& voice, int numSamples) const
{
    // preRollWriteIndex points at the oldest sample, so the newest numSamples end just before it.
    const auto firstIndex = preRollWriteIndex - numSamples + preRollSamples;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto readIndex = (firstIndex + i) % preRollSamples;
        for (int channel = 0; channel < numPreparedChannels; ++channel)
            voice.capture.setSample (channel, i, preRoll.getSample (channel, readIndex));
    }
}

void RepeaterEngine::pushPreRoll (const Frame& dry)
{
    for (int channel = 0; channel < numPreparedChannels; ++channel)
        preRoll.setSample (channel, preRollWriteIndex, dry[static_cast<size_t> (channel)]);

    preRollWriteIndex = (preRollWriteIndex + 1) % preRollSamples;
}

RepeaterEngine::Frame RepeaterEngine::renderVoice (Voice& voice, const Frame& dry, float pingPong, std::span<const float> curveTable) const
{
    if (! voice.active)
        return {};

    captureSample (voice, dry);

    Frame wet {};
    if (voice.isRepeating())
    {
        const auto gain = wetGain (voice, curveTable);
        const auto repeat = readRepeat (voice, pingPong);
        wet = { repeat[0] * gain, repeat[1] * gain };
    }

    advanceVoice (voice);
    return wet;
}

void RepeaterEngine::captureSample (Voice& voice, const Frame& dry) const
{
    if (! voice.isCapturing())
        return;

    for (int channel = 0; channel < numPreparedChannels; ++channel)
        voice.capture.setSample (channel, voice.position, dry[static_cast<size_t> (channel)]);
}

RepeaterEngine::Frame RepeaterEngine::readRepeat (const Voice& voice, float pingPong) const
{
    const auto slicePosition = voice.position % voice.sliceSamples;
    const auto left = voice.capture.getSample (0, slicePosition);

    if (numPreparedChannels == 1)
        return { left, 0.0f };

    const auto right = voice.capture.getSample (1, slicePosition);
    const auto mono = 0.5f * (left + right);
    const auto repeatIsOdd = (voice.position / voice.sliceSamples) % 2 == 1;
    const Frame pingPongFrame = repeatIsOdd ? Frame { mono, 0.0f } : Frame { 0.0f, mono };

    return { left + (pingPongFrame[0] - left) * pingPong, right + (pingPongFrame[1] - right) * pingPong };
}

float RepeaterEngine::wetGain (const Voice& voice, std::span<const float> curveTable) const
{
    const auto normalisedTime = static_cast<double> (voice.position) / static_cast<double> (voice.lengthSamples);

    return curveGainAt (curveTable, normalisedTime)
           * edgeFadeGain (voice.position % voice.sliceSamples, voice.sliceSamples, voice.edgeFadeSamples)
           * endFadeGain (voice.lengthSamples - voice.position, voice.edgeFadeSamples)
           * restartFadeGain (voice);
}

float RepeaterEngine::restartFadeGain (const Voice& voice) const
{
    if (! voice.restartFading)
        return 1.0f;

    // Counts down from (n-1)/n to exactly 0 on the last sample.
    return static_cast<float> (voice.restartFadeRemaining - 1) / static_cast<float> (restartFadeSamples);
}

void RepeaterEngine::advanceVoice (Voice& voice) const
{
    ++voice.position;

    if (voice.restartFading && --voice.restartFadeRemaining <= 0)
        voice.active = false;

    if (voice.position >= voice.lengthSamples)
        voice.active = false;
}

void RepeaterEngine::publishPlayhead()
{
    const auto& voice = voices[static_cast<size_t> (activeVoiceIndex)];
    const auto position = voice.active
                              ? static_cast<float> (voice.position) / static_cast<float> (voice.lengthSamples)
                              : -1.0f;

    playheadPosition.store (position, std::memory_order_relaxed);
}

float RepeaterEngine::curveGainAt (std::span<const float> curveTable, double normalisedTime)
{
    if (curveTable.empty())
        return 1.0f;

    if (curveTable.size() == 1)
        return curveTable.front();

    const auto lastIndex = curveTable.size() - 1;
    const auto x = juce::jlimit (0.0, 1.0, normalisedTime) * static_cast<double> (lastIndex);
    const auto index = juce::jmin (static_cast<size_t> (x), lastIndex - 1);
    const auto fraction = static_cast<float> (x - static_cast<double> (index));

    return curveTable[index] + (curveTable[index + 1] - curveTable[index]) * fraction;
}

float RepeaterEngine::edgeFadeGain (int slicePosition, int sliceSamples, int fadeSamples)
{
    // 0 on the first and last sample of every repeat, so consecutive repeats meet at silence.
    const auto fadeIn = static_cast<float> (slicePosition) / static_cast<float> (fadeSamples);
    const auto fadeOut = static_cast<float> (sliceSamples - 1 - slicePosition) / static_cast<float> (fadeSamples);
    return juce::jmin (1.0f, fadeIn, fadeOut);
}

float RepeaterEngine::endFadeGain (int samplesToEnd, int fadeSamples)
{
    // samplesToEnd is 1 on the last sample before length, where the gain reaches 0.
    return juce::jmin (1.0f, static_cast<float> (samplesToEnd - 1) / static_cast<float> (fadeSamples));
}
