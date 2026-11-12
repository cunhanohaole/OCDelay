#include "RepeaterEngine.h"
#include "PitchMapping.h"

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
    pingPongWidth.reset (sampleRate, kParameterRampSeconds);

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
    pingPongWidth.setCurrentAndTargetValue (pingPongWidth.getTargetValue());
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

void RepeaterEngine::setPingPongWidth (float newWidth)
{
    pingPongWidth.setTargetValue (juce::jlimit (0.0f, 1.0f, newWidth));
}

void RepeaterEngine::setPitchRange (float rangeSemitones, bool bipolar)
{
    pitchRangeSemitones = rangeSemitones;
    pitchBipolar = bipolar;
}

void RepeaterEngine::process (juce::AudioBuffer<float>& buffer, std::span<const float> volumeTable, std::span<const float> pitchTable)
{
    jassert (sampleRate > 0.0); // prepare() must be called first
    jassert (buffer.getNumSamples() <= maxBlockSamples);
    jassert (buffer.getNumChannels() >= numPreparedChannels);
    jassert (! volumeTable.empty());

    const Tables tables { volumeTable, pitchTable };
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        processFrame (buffer, i, tables);

    publishPlayhead();
}

void RepeaterEngine::processFrame (juce::AudioBuffer<float>& buffer, int index, const Tables& tables)
{
    const auto dry = readFrame (buffer, index);
    detectHit (dry);

    const Panning panning { pingPongAmount.getNextValue(), pingPongWidth.getNextValue() };
    Frame wet {};

    for (auto& voice : voices)
    {
        const auto voiceWet = renderVoice (voice, dry, panning, tables);
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
    setRepeatRate (voice, 1.0); // the original slice plays at its own pitch

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

RepeaterEngine::Frame RepeaterEngine::renderVoice (Voice& voice, const Frame& dry, const Panning& panning, const Tables& tables) const
{
    if (! voice.active)
        return {};

    captureSample (voice, dry);

    Frame wet {};
    if (voice.isRepeating())
    {
        if (voice.position % voice.sliceSamples == 0)
            startRepeat (voice, tables.pitch);

        const auto gain = wetGain (voice, tables.volume);
        const auto repeat = readRepeat (voice, panning);
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

// Holds the pitch curve's value at the start of the repeat for the whole repeat
void RepeaterEngine::startRepeat (Voice& voice, std::span<const float> pitchTable) const
{
    if (pitchTable.empty())
    {
        setRepeatRate (voice, 1.0);
        return;
    }

    const auto normalisedTime = static_cast<double> (voice.position) / static_cast<double> (voice.lengthSamples);
    const auto semitones = PitchMapping::toSemitones (lookUp (pitchTable, normalisedTime), pitchRangeSemitones, pitchBipolar);
    setRepeatRate (voice, PitchMapping::semitonesToRate (semitones));
}

// Faster playback runs out of slice early; the edge fades follow the audio that is played
void RepeaterEngine::setRepeatRate (Voice& voice, double rate) const
{
    jassert (rate > 0.0);
    voice.rate = rate;

    const auto lastReadable = static_cast<double> (voice.sliceSamples - 1);
    voice.playedSamples = juce::jmin (voice.sliceSamples, static_cast<int> (lastReadable / rate) + 1);
    voice.edgeFadeSamples = juce::jlimit (1, maxEdgeFadeSamples, voice.playedSamples / 2);
}

RepeaterEngine::Frame RepeaterEngine::readRepeat (const Voice& voice, const Panning& panning) const
{
    const auto slicePosition = voice.position % voice.sliceSamples;
    if (slicePosition >= voice.playedSamples)
        return {};

    const auto readPosition = static_cast<double> (slicePosition) * voice.rate;
    const auto left = readInterpolated (voice.capture, 0, readPosition, voice.sliceSamples);

    if (numPreparedChannels == 1)
        return { left, 0.0f };

    const auto right = readInterpolated (voice.capture, 1, readPosition, voice.sliceSamples);
    const auto mono = 0.5f * (left + right);
    const auto farSide = mono * (1.0f - panning.width);
    const auto repeatIsOdd = (voice.position / voice.sliceSamples) % 2 == 1;
    const Frame pingPongFrame = repeatIsOdd ? Frame { mono, farSide } : Frame { farSide, mono };

    return { left + (pingPongFrame[0] - left) * panning.amount, right + (pingPongFrame[1] - right) * panning.amount };
}

float RepeaterEngine::wetGain (const Voice& voice, std::span<const float> volumeTable) const
{
    const auto normalisedTime = static_cast<double> (voice.position) / static_cast<double> (voice.lengthSamples);

    return lookUp (volumeTable, normalisedTime)
           * edgeFadeGain (voice.position % voice.sliceSamples, voice.playedSamples, voice.edgeFadeSamples)
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

float RepeaterEngine::lookUp (std::span<const float> table, double normalisedTime)
{
    if (table.empty())
        return 1.0f;

    if (table.size() == 1)
        return table.front();

    const auto lastIndex = table.size() - 1;
    const auto x = juce::jlimit (0.0, 1.0, normalisedTime) * static_cast<double> (lastIndex);
    const auto index = juce::jmin (static_cast<size_t> (x), lastIndex - 1);
    const auto fraction = static_cast<float> (x - static_cast<double> (index));

    return table[index] + (table[index + 1] - table[index]) * fraction;
}

// Catmull-Rom cubic between samples; exact on whole-sample positions
float RepeaterEngine::readInterpolated (const juce::AudioBuffer<float>& capture, int channel, double position, int sliceSamples)
{
    const auto index = static_cast<int> (position);
    const auto t = static_cast<float> (position - static_cast<double> (index));
    const auto sampleAt = [&] (int i) { return capture.getSample (channel, juce::jlimit (0, sliceSamples - 1, i)); };

    const auto y0 = sampleAt (index - 1);
    const auto y1 = sampleAt (index);
    const auto y2 = sampleAt (index + 1);
    const auto y3 = sampleAt (index + 2);

    const auto c1 = 0.5f * (y2 - y0);
    const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

float RepeaterEngine::edgeFadeGain (int slicePosition, int playedSamples, int fadeSamples)
{
    // 0 on the first and last played sample of every repeat, so consecutive repeats meet at silence.
    const auto fadeIn = static_cast<float> (slicePosition) / static_cast<float> (fadeSamples);
    const auto fadeOut = static_cast<float> (playedSamples - 1 - slicePosition) / static_cast<float> (fadeSamples);
    return juce::jlimit (0.0f, 1.0f, juce::jmin (fadeIn, fadeOut));
}

float RepeaterEngine::endFadeGain (int samplesToEnd, int fadeSamples)
{
    // samplesToEnd is 1 on the last sample before length, where the gain reaches 0.
    return juce::jmin (1.0f, static_cast<float> (samplesToEnd - 1) / static_cast<float> (fadeSamples));
}
