#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <dsp/RepeaterEngine.h>
#include <memory>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;
    constexpr double kMaxSliceSeconds = 8.0;
    constexpr int kSlice = 4800;   // 100 ms
    constexpr int kLength = 48000; // 1 s
    constexpr int kHitSample = 1000;

    struct EngineSettings
    {
        int sliceSamples = kSlice;
        int lengthSamples = kLength;
        float mix = 1.0f;
        bool pingPong = false;
        int numChannels = 2;
    };

    const std::vector<float> flatCurve (2048, 1.0f);

    // Prepared engine with smoothers already at their targets
    // (heap-allocated because the engine holds an atomic and can't be moved)
    std::unique_ptr<RepeaterEngine> makeEngine (const EngineSettings& settings)
    {
        auto engine = std::make_unique<RepeaterEngine>();
        engine->setSliceSamples (settings.sliceSamples);
        engine->setLengthSamples (settings.lengthSamples);
        engine->setThresholdDb (-30.0f);
        engine->setMix (settings.mix);
        engine->setPingPong (settings.pingPong);
        engine->prepare (kSampleRate, kBlockSize, settings.numChannels, kMaxSliceSeconds);
        return engine;
    }

    void processInBlocks (RepeaterEngine& engine, juce::AudioBuffer<float>& buffer, const std::vector<float>& curve = flatCurve)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += kBlockSize)
        {
            const auto length = juce::jmin (kBlockSize, buffer.getNumSamples() - start);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);
            engine.process (block, curve);
        }
    }

    juce::AudioBuffer<float> makeSilence (int numChannels, int numSamples)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        buffer.clear();
        return buffer;
    }

    juce::AudioBuffer<float> makeImpulse (int numChannels, int numSamples, int impulseChannel = -1)
    {
        auto buffer = makeSilence (numChannels, numSamples);
        for (int channel = 0; channel < numChannels; ++channel)
            if (impulseChannel < 0 || channel == impulseChannel)
                buffer.setSample (channel, kHitSample, 1.0f);
        return buffer;
    }

    // A 0.5 plateau with 500-sample linear ramps at both ends, so the source itself never steps
    void addSmoothBurst (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        constexpr float kRampSamples = 500.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto envelope = juce::jmin (1.0f, (float) i / kRampSamples, (float) (numSamples - i) / kRampSamples);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample (channel, startSample + i, 0.5f * envelope);
        }
    }

    float maxAbsInRange (const juce::AudioBuffer<float>& buffer, int channel, int start, int end)
    {
        float result = 0.0f;
        for (int i = start; i < end; ++i)
            result = juce::jmax (result, std::abs (buffer.getSample (channel, i)));
        return result;
    }

    float maxSampleStep (const juce::AudioBuffer<float>& buffer, int channel)
    {
        float result = 0.0f;
        for (int i = 1; i < buffer.getNumSamples(); ++i)
            result = juce::jmax (result, std::abs (buffer.getSample (channel, i) - buffer.getSample (channel, i - 1)));
        return result;
    }

    // Repeat k of a hit at kHitSample, as an output sample index
    int repeatSample (int k, int sliceSamples = kSlice) { return kHitSample + k * sliceSamples; }
}

TEST_CASE ("A short burst repeats every slice with equal amplitude until length", "[RepeaterEngine]")
{
    auto engine = makeEngine ({});
    const auto origin = kHitSample - engine->getPreRollSamples();
    auto buffer = makeImpulse (2, origin + kLength + 10000);

    processInBlocks (*engine, buffer);

    // The hit sits pre-roll samples into the slice, so the last repeat that fits starts at k = 9
    for (int k = 1; k <= 9; ++k)
        for (int channel = 0; channel < 2; ++channel)
            CHECK_THAT (buffer.getSample (channel, repeatSample (k)), WithinAbs (1.0, 1.0e-6));

    for (int k = 1; k <= 9; ++k)
        CHECK_THAT (maxAbsInRange (buffer, 0, repeatSample (k) + 1, repeatSample (k + 1)), WithinAbs (0.0, 1.0e-6));

    CHECK_THAT (maxAbsInRange (buffer, 0, origin + kLength, buffer.getNumSamples()), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (maxAbsInRange (buffer, 1, origin + kLength, buffer.getNumSamples()), WithinAbs (0.0, 1.0e-6));
}

TEST_CASE ("No wet signal plays during the original slice", "[RepeaterEngine]")
{
    auto engine = makeEngine ({});
    auto buffer = makeImpulse (2, kHitSample + 2 * kSlice);

    processInBlocks (*engine, buffer);

    CHECK_THAT (maxAbsInRange (buffer, 0, 0, repeatSample (1)), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (maxAbsInRange (buffer, 1, 0, repeatSample (1)), WithinAbs (0.0, 1.0e-6));
}

TEST_CASE ("The slice includes pre-roll from before the hit was detected", "[RepeaterEngine]")
{
    auto engine = makeEngine ({});
    REQUIRE (engine->getPreRollSamples() == 240);

    auto buffer = makeImpulse (2, kHitSample + 2 * kSlice);
    constexpr int kLeadIn = 50; // inside the pre-roll but past the repeat fade-in
    constexpr float kSubThreshold = 0.01f;
    buffer.setSample (0, kHitSample - kLeadIn, kSubThreshold);
    buffer.setSample (1, kHitSample - kLeadIn, kSubThreshold);

    processInBlocks (*engine, buffer);

    CHECK_THAT (buffer.getSample (0, repeatSample (1) - kLeadIn), WithinAbs (kSubThreshold, 1.0e-6));
    CHECK_THAT (buffer.getSample (0, repeatSample (1)), WithinAbs (1.0, 1.0e-6));
}

TEST_CASE ("A ramp-down curve sets each repeat's amplitude", "[RepeaterEngine]")
{
    auto engine = makeEngine ({});
    const auto preRoll = engine->getPreRollSamples();
    auto buffer = makeImpulse (2, kHitSample + kLength);
    const std::vector<float> rampDown { 1.0f, 0.0f };

    processInBlocks (*engine, buffer, rampDown);

    for (int k = 1; k <= 9; ++k)
    {
        const auto expected = 1.0 - (double) (preRoll + k * kSlice) / kLength;
        CHECK_THAT (buffer.getSample (0, repeatSample (k)), WithinAbs (expected, 1.0e-5));
    }
}

TEST_CASE ("A hit mid-tail restarts without a jump", "[RepeaterEngine]")
{
    auto engine = makeEngine ({});
    const auto preRoll = engine->getPreRollSamples();
    const auto secondHit = kHitSample + 3 * kSlice + kSlice / 2; // mid-plateau of repeat 3
    auto buffer = makeSilence (2, secondHit + kLength);
    addSmoothBurst (buffer, kHitSample, kSlice);
    addSmoothBurst (buffer, secondHit, kSlice);

    processInBlocks (*engine, buffer);

    // The old tail was at 0.5 when the hit landed; a hard cut would step by 0.5
    CHECK (maxSampleStep (buffer, 0) < 0.01f);
    CHECK (maxSampleStep (buffer, 1) < 0.01f);

    // Old tail is gone once the restart fade is over, and the new capture is silent
    const auto newRepeatStart = secondHit - preRoll + kSlice;
    CHECK_THAT (maxAbsInRange (buffer, 0, secondHit + 200, newRepeatStart), WithinAbs (0.0, 1.0e-6));

    // The new slice repeats from the second hit
    CHECK_THAT (buffer.getSample (0, secondHit + kSlice + kSlice / 2), WithinAbs (0.5, 1.0e-3));
}

TEST_CASE ("Slice edges don't click on a continuous sine", "[RepeaterEngine]")
{
    constexpr int kOddSlice = 4801; // the sine's phase doesn't line up at slice boundaries
    auto engine = makeEngine ({ .sliceSamples = kOddSlice });
    auto buffer = makeSilence (2, kHitSample + kLength);

    constexpr float kFrequency = 440.0f;
    constexpr float kAmplitude = 0.5f;
    for (int i = kHitSample; i < buffer.getNumSamples(); ++i)
        for (int channel = 0; channel < 2; ++channel)
            buffer.setSample (channel, i, kAmplitude * std::sin (juce::MathConstants<float>::twoPi * kFrequency * (float) (i - kHitSample) / (float) kSampleRate));

    processInBlocks (*engine, buffer);

    const auto naturalStep = juce::MathConstants<float>::twoPi * kFrequency / (float) kSampleRate * kAmplitude;
    const auto fadeStep = kAmplitude / (float) engine->getEdgeFadeSamples();

    CHECK (maxAbsInRange (buffer, 0, 0, buffer.getNumSamples()) > 0.4f);
    CHECK (maxSampleStep (buffer, 0) < naturalStep + fadeStep + 1.0e-3f);
    CHECK (maxSampleStep (buffer, 1) < naturalStep + fadeStep + 1.0e-3f);
}

TEST_CASE ("Ping-pong sends odd repeats left and even repeats right", "[RepeaterEngine]")
{
    auto engine = makeEngine ({ .pingPong = true });
    auto buffer = makeImpulse (2, repeatSample (5), 0); // hit in the left channel only

    processInBlocks (*engine, buffer);

    // Mono sum of (1, 0) is 0.5
    for (int k = 1; k <= 4; ++k)
    {
        const auto isOdd = k % 2 == 1;
        CHECK_THAT (buffer.getSample (0, repeatSample (k)), WithinAbs (isOdd ? 0.5 : 0.0, 1.0e-6));
        CHECK_THAT (buffer.getSample (1, repeatSample (k)), WithinAbs (isOdd ? 0.0 : 0.5, 1.0e-6));
    }
}

TEST_CASE ("Mix at 0 passes the input through untouched", "[RepeaterEngine]")
{
    auto engine = makeEngine ({ .mix = 0.0f });
    juce::Random random (42);
    auto buffer = makeSilence (2, kHitSample + kLength);
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (channel, i, random.nextFloat() * 2.0f - 1.0f);

    juce::AudioBuffer<float> input;
    input.makeCopyOf (buffer);

    processInBlocks (*engine, buffer);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            REQUIRE_THAT (buffer.getSample (channel, i), WithinAbs (input.getSample (channel, i), 1.0e-7));
}

TEST_CASE ("A mono engine repeats and ignores ping-pong", "[RepeaterEngine]")
{
    auto engine = makeEngine ({ .pingPong = true, .numChannels = 1 });
    auto buffer = makeImpulse (1, repeatSample (4));

    processInBlocks (*engine, buffer);

    for (int k = 1; k <= 3; ++k)
        CHECK_THAT (buffer.getSample (0, repeatSample (k)), WithinAbs (1.0, 1.0e-6));
}

TEST_CASE ("The playhead reports the active tail position", "[RepeaterEngine]")
{
    auto engine = makeEngine ({});
    CHECK_THAT (engine->getPlayheadPosition(), WithinAbs (-1.0, 1.0e-6));

    const auto origin = kHitSample - engine->getPreRollSamples();
    auto buffer = makeImpulse (2, origin + kLength / 2);
    processInBlocks (*engine, buffer);
    CHECK_THAT (engine->getPlayheadPosition(), WithinAbs (0.5, 1.0e-6));

    auto rest = makeSilence (2, kLength / 2);
    processInBlocks (*engine, rest);
    CHECK_THAT (engine->getPlayheadPosition(), WithinAbs (-1.0, 1.0e-6));
}
