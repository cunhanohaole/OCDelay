#include "RepeatVoice.h"
#include "PitchMapping.h"

namespace
{
    float lookUp (std::span<const float> table, double normalisedTime)
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

    // 0 on the first and last played sample of a repeat, so repeats start and end at silence
    float edgeFadeGain (int headPosition, int playedSamples, int fadeSamples)
    {
        const auto fadeIn = static_cast<float> (headPosition) / static_cast<float> (fadeSamples);
        const auto fadeOut = static_cast<float> (playedSamples - 1 - headPosition) / static_cast<float> (fadeSamples);
        return juce::jlimit (0.0f, 1.0f, juce::jmin (fadeIn, fadeOut));
    }
}

void RepeatVoice::prepare (int newNumChannels, int maxSliceSamples, int newMaxEdgeFadeSamples, int newRestartFadeSamples)
{
    numChannels = newNumChannels;
    maxEdgeFadeSamples = newMaxEdgeFadeSamples;
    restartFadeSamples = newRestartFadeSamples;
    capture.setSize (numChannels, maxSliceSamples);
    active = false;
}

void RepeatVoice::start (const Settings& newSettings)
{
    jassert (newSettings.sliceSamples > 0 && newSettings.intervalSamples > 0 && newSettings.lengthSamples > 0);
    jassert (newSettings.sliceSamples <= capture.getNumSamples());

    settings = newSettings;
    position = settings.preRollSamples;

    // Repeats that would start inside the pre-roll (before the hit was heard) are skipped
    nextRepeatStart = settings.intervalSamples * (position / settings.intervalSamples + 1);

    for (auto& head : heads)
        head.active = false;

    nextHeadSlot = 0;
    restartFading = false;
    restartFadeRemaining = 0;
    active = true;
}

void RepeatVoice::beginRestartFade()
{
    if (! active || restartFading)
        return;

    restartFading = true;
    restartFadeRemaining = restartFadeSamples;
}

void RepeatVoice::writeCapture (int index, const StereoFrame& frame)
{
    for (int channel = 0; channel < numChannels; ++channel)
        capture.setSample (channel, index, frame[static_cast<size_t> (channel)]);
}

StereoFrame RepeatVoice::render (const StereoFrame& dry, const Context& context)
{
    if (! active)
        return {};

    if (isCapturing())
        writeCapture (position, dry);

    if (position == nextRepeatStart && position < settings.lengthSamples)
        startHead (context);

    StereoFrame wet {};
    for (const auto& head : heads)
    {
        if (! head.active)
            continue;

        const auto headWet = renderHead (head, context);
        wet[0] += headWet[0];
        wet[1] += headWet[1];
    }

    const auto gain = sharedGain (context);
    advance();
    return { wet[0] * gain, wet[1] * gain };
}

// Holds the pitch curve's value at the repeat's start for the whole repeat
void RepeatVoice::startHead (const Context& context)
{
    auto& head = heads[static_cast<size_t> (nextHeadSlot)];
    nextHeadSlot = (nextHeadSlot + 1) % kMaxHeads;

    head.startPosition = position;
    head.repeatIndex = position / settings.intervalSamples;
    head.rate = rateAt (position, context);
    head.playedSamples = playedSamplesFor (position, head.rate);
    head.edgeFadeSamples = juce::jlimit (1, maxEdgeFadeSamples, head.playedSamples / 2);
    head.active = true;

    nextRepeatStart += settings.intervalSamples;
}

double RepeatVoice::rateAt (int voicePosition, const Context& context) const
{
    if (context.pitchTable.empty())
        return 1.0;

    const auto normalisedTime = static_cast<double> (voicePosition) / static_cast<double> (settings.lengthSamples);
    const auto curveValue = lookUp (context.pitchTable, normalisedTime);
    return PitchMapping::semitonesToRate (PitchMapping::toSemitones (curveValue, context.pitchRangeSemitones, context.pitchBipolar));
}

// How long a repeat holds audio: until the slice runs out at its rate, cut at the next start
// (or the 16th next with overlap), and never past audio that hasn't been captured yet
int RepeatVoice::playedSamplesFor (int startPosition, double rate) const
{
    const auto sliceAtRate = static_cast<int> (static_cast<double> (settings.sliceSamples - 1) / rate) + 1;
    const auto window = settings.intervalSamples * (settings.overlap ? kMaxHeads : 1);
    auto played = juce::jmin (sliceAtRate, window);

    // Reading at rate r from a slice still being captured: the read head (plus the two samples
    // the interpolator looks ahead) must stay behind the write head
    if (startPosition < settings.sliceSamples && rate > 1.0)
    {
        const auto catchUp = static_cast<int> (static_cast<double> (juce::jmax (0, startPosition - 2)) / (rate - 1.0));
        if (startPosition + catchUp < settings.sliceSamples)
            played = juce::jmin (played, catchUp + 1);
    }

    return juce::jmax (1, played);
}

StereoFrame RepeatVoice::renderHead (const Head& head, const Context& context) const
{
    const auto headPosition = position - head.startPosition;
    const auto readPosition = static_cast<double> (headPosition) * head.rate;
    const auto fade = edgeFadeGain (headPosition, head.playedSamples, head.edgeFadeSamples);

    const auto left = readInterpolated (0, readPosition) * fade;
    if (numChannels == 1)
        return { left, 0.0f };

    const auto right = readInterpolated (1, readPosition) * fade;
    return pan (left, right, head.repeatIndex, context);
}

// Ping-pong: odd repeats left, even repeats right, from the mono sum; Width scales the far side
StereoFrame RepeatVoice::pan (float left, float right, int repeatIndex, const Context& context) const
{
    const auto mono = 0.5f * (left + right);
    const auto farSide = mono * (1.0f - context.pingPongWidth);
    const StereoFrame pingPong = repeatIndex % 2 == 1 ? StereoFrame { mono, farSide } : StereoFrame { farSide, mono };
    const auto amount = context.pingPongAmount;

    return { left + (pingPong[0] - left) * amount, right + (pingPong[1] - right) * amount };
}

// Volume curve, the fade-out ending exactly at length, and the retrigger fade
float RepeatVoice::sharedGain (const Context& context) const
{
    const auto normalisedTime = static_cast<double> (position) / static_cast<double> (settings.lengthSamples);
    const auto samplesToEnd = settings.lengthSamples - position;
    const auto endFade = juce::jmin (1.0f, static_cast<float> (samplesToEnd - 1) / static_cast<float> (maxEdgeFadeSamples));

    // Counts down from (n-1)/n to exactly 0 on the last sample
    const auto restartFade = restartFading ? static_cast<float> (restartFadeRemaining - 1) / static_cast<float> (restartFadeSamples) : 1.0f;

    return lookUp (context.volumeTable, normalisedTime) * juce::jmax (0.0f, endFade) * restartFade;
}

void RepeatVoice::advance()
{
    ++position;

    for (auto& head : heads)
        if (head.active && position - head.startPosition >= head.playedSamples)
            head.active = false;

    if (restartFading && --restartFadeRemaining <= 0)
        active = false;

    if (position >= settings.lengthSamples)
        active = false;
}

float RepeatVoice::getPlayheadPosition() const
{
    return active ? static_cast<float> (position) / static_cast<float> (settings.lengthSamples) : -1.0f;
}

// Catmull-Rom cubic between samples; exact on whole-sample positions
float RepeatVoice::readInterpolated (int channel, double readPosition) const
{
    const auto index = static_cast<int> (readPosition);
    const auto t = static_cast<float> (readPosition - static_cast<double> (index));
    const auto lastIndex = settings.sliceSamples - 1;
    const auto sampleAt = [&] (int i) { return capture.getSample (channel, juce::jlimit (0, lastIndex, i)); };

    const auto y0 = sampleAt (index - 1);
    const auto y1 = sampleAt (index);
    const auto y2 = sampleAt (index + 1);
    const auto y3 = sampleAt (index + 2);

    const auto c1 = 0.5f * (y2 - y0);
    const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}
