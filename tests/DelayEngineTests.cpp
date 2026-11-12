#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <dsp/DelayEngine.h>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;
    constexpr double kMaxDelaySeconds = 2.0;

    struct EngineSettings
    {
        float delayMs = 10.0f;
        float feedback = 0.0f;
        float mix = 1.0f;
        bool pingPong = false;
        int numChannels = 2;
    };

    // Prepared engine with smoothers already at their targets
    DelayEngine makeEngine (const EngineSettings& settings)
    {
        DelayEngine engine;
        engine.setDelayTimeMs (settings.delayMs);
        engine.setFeedback (settings.feedback);
        engine.setMix (settings.mix);
        engine.setPingPong (settings.pingPong);
        engine.prepare (kSampleRate, kBlockSize, settings.numChannels, kMaxDelaySeconds);
        engine.reset();
        return engine;
    }

    // Runs the whole buffer through the engine in host-sized blocks
    void processInBlocks (DelayEngine& engine, juce::AudioBuffer<float>& buffer)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += kBlockSize)
        {
            const auto length = juce::jmin (kBlockSize, buffer.getNumSamples() - start);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);
            engine.process (block);
        }
    }

    juce::AudioBuffer<float> makeImpulse (int numChannels, int numSamples, int impulseChannel = -1)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        buffer.clear();

        for (int channel = 0; channel < numChannels; ++channel)
            if (impulseChannel < 0 || channel == impulseChannel)
                buffer.setSample (channel, 0, 1.0f);

        return buffer;
    }

    void fillSine (juce::AudioBuffer<float>& buffer, int startSample, float frequency, float amplitude)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto phase = juce::MathConstants<double>::twoPi * frequency * (startSample + i) / kSampleRate;
                buffer.setSample (channel, i, amplitude * (float) std::sin (phase));
            }
    }

    float maxSampleToSampleJump (const juce::AudioBuffer<float>& buffer, int channel)
    {
        float maxJump = 0.0f;
        for (int i = 1; i < buffer.getNumSamples(); ++i)
            maxJump = juce::jmax (maxJump, std::abs (buffer.getSample (channel, i) - buffer.getSample (channel, i - 1)));
        return maxJump;
    }

    int delayMsToSamples (float delayMs)
    {
        return juce::roundToInt (delayMs * kSampleRate / 1000.0);
    }
}

using Catch::Matchers::WithinAbs;

TEST_CASE ("Impulse with no feedback produces a single repeat", "[DelayEngine]")
{
    auto engine = makeEngine ({ .delayMs = 10.0f });
    auto buffer = makeImpulse (2, 4000);
    processInBlocks (engine, buffer);

    const auto delay = delayMsToSamples (10.0f);
    for (int channel = 0; channel < 2; ++channel)
    {
        CHECK_THAT (buffer.getSample (channel, delay), WithinAbs (1.0, 1.0e-4));
        CHECK_THAT (buffer.getSample (channel, 2 * delay), WithinAbs (0.0, 1.0e-4));
        CHECK_THAT (buffer.getMagnitude (channel, delay + 4, 4000 - delay - 4), WithinAbs (0.0, 1.0e-4));
    }
}

TEST_CASE ("Feedback produces decaying repeats", "[DelayEngine]")
{
    auto engine = makeEngine ({ .delayMs = 10.0f, .feedback = 0.5f });
    auto buffer = makeImpulse (2, 4000);
    processInBlocks (engine, buffer);

    const auto delay = delayMsToSamples (10.0f);
    CHECK_THAT (buffer.getSample (0, delay), WithinAbs (1.0, 1.0e-4));
    CHECK_THAT (buffer.getSample (0, 2 * delay), WithinAbs (0.5, 1.0e-4));
    CHECK_THAT (buffer.getSample (0, 3 * delay), WithinAbs (0.25, 1.0e-4));
}

TEST_CASE ("Feedback is capped inside the engine", "[DelayEngine]")
{
    auto engine = makeEngine ({ .delayMs = 10.0f, .feedback = 1.5f });
    auto buffer = makeImpulse (2, 4000);
    processInBlocks (engine, buffer);

    const auto delay = delayMsToSamples (10.0f);
    CHECK_THAT (buffer.getSample (0, 2 * delay), WithinAbs (DelayEngine::kMaxFeedback, 1.0e-4));
}

TEST_CASE ("Maximum feedback stays stable on noise", "[DelayEngine]")
{
    const auto pingPong = GENERATE (false, true);
    auto engine = makeEngine ({ .delayMs = 1.0f, .feedback = 1.0f, .pingPong = pingPong });

    juce::Random random (42);
    juce::AudioBuffer<float> buffer (2, (int) (10.0 * kSampleRate));
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (channel, i, random.nextFloat() - 0.5f);

    processInBlocks (engine, buffer);

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);
        CHECK (std::all_of (samples, samples + buffer.getNumSamples(), [] (float s) { return std::isfinite (s); }));
        CHECK (buffer.getMagnitude (channel, 0, buffer.getNumSamples()) < 20.0f);
    }
}

TEST_CASE ("Mix at zero passes the input through untouched", "[DelayEngine]")
{
    auto engine = makeEngine ({ .delayMs = 10.0f, .feedback = 0.5f, .mix = 0.0f });
    juce::AudioBuffer<float> buffer (2, 4000);
    fillSine (buffer, 0, 440.0f, 0.5f);
    juce::AudioBuffer<float> original (buffer);

    processInBlocks (engine, buffer);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            REQUIRE (juce::exactlyEqual (buffer.getSample (channel, i), original.getSample (channel, i)));
}

TEST_CASE ("Ping-pong alternates repeats between left and right", "[DelayEngine]")
{
    auto engine = makeEngine ({ .delayMs = 10.0f, .feedback = 0.5f, .pingPong = true });
    auto buffer = makeImpulse (2, 4000, 0);
    processInBlocks (engine, buffer);

    // Input is summed to mono (0.5), enters on the left, then bounces
    const auto delay = delayMsToSamples (10.0f);
    CHECK_THAT (buffer.getSample (0, delay), WithinAbs (0.5, 1.0e-4));
    CHECK_THAT (buffer.getSample (1, delay), WithinAbs (0.0, 1.0e-4));
    CHECK_THAT (buffer.getSample (0, 2 * delay), WithinAbs (0.0, 1.0e-4));
    CHECK_THAT (buffer.getSample (1, 2 * delay), WithinAbs (0.25, 1.0e-4));
    CHECK_THAT (buffer.getSample (0, 3 * delay), WithinAbs (0.125, 1.0e-4));
}

TEST_CASE ("Changing delay time glides instead of jumping", "[DelayEngine]")
{
    auto engine = makeEngine ({ .delayMs = 100.0f });
    constexpr float frequency = 220.0f;
    constexpr float amplitude = 0.5f;

    // Fill the delay line with a steady sine first
    const auto warmUpSamples = (int) (0.5 * kSampleRate);
    juce::AudioBuffer<float> warmUp (2, warmUpSamples);
    fillSine (warmUp, 0, frequency, amplitude);
    processInBlocks (engine, warmUp);

    engine.setDelayTimeMs (300.0f);
    juce::AudioBuffer<float> buffer (2, (int) (0.3 * kSampleRate));
    fillSine (buffer, warmUpSamples, frequency, amplitude);
    processInBlocks (engine, buffer);

    // A hard jump in read position would produce a step close to the full amplitude
    CHECK (maxSampleToSampleJump (buffer, 0) < 0.1f);
    CHECK (maxSampleToSampleJump (buffer, 1) < 0.1f);
}

TEST_CASE ("Toggling ping-pong does not click", "[DelayEngine]")
{
    auto engine = makeEngine ({ .delayMs = 50.0f, .feedback = 0.5f });
    constexpr float frequency = 220.0f;
    constexpr float amplitude = 0.5f;

    const auto warmUpSamples = (int) kSampleRate;
    juce::AudioBuffer<float> warmUp (2, warmUpSamples);
    fillSine (warmUp, 0, frequency, amplitude);
    processInBlocks (engine, warmUp);

    engine.setPingPong (true);
    juce::AudioBuffer<float> buffer (2, (int) (0.5 * kSampleRate));
    fillSine (buffer, warmUpSamples, frequency, amplitude);
    processInBlocks (engine, buffer);

    CHECK (maxSampleToSampleJump (buffer, 0) < 0.15f);
    CHECK (maxSampleToSampleJump (buffer, 1) < 0.15f);
}

TEST_CASE ("Mono engine delays a single channel", "[DelayEngine]")
{
    auto engine = makeEngine ({ .delayMs = 10.0f, .feedback = 0.5f, .pingPong = true, .numChannels = 1 });
    auto buffer = makeImpulse (1, 4000);
    processInBlocks (engine, buffer);

    // Ping-pong has nowhere to bounce on mono, so it behaves like a plain delay
    const auto delay = delayMsToSamples (10.0f);
    CHECK_THAT (buffer.getSample (0, delay), WithinAbs (1.0, 1.0e-4));
    CHECK_THAT (buffer.getSample (0, 2 * delay), WithinAbs (0.5, 1.0e-4));
}
