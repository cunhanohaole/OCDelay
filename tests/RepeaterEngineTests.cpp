#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
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
        float pingPongWidth = 1.0f;
        int intervalSamples = 0; // 0 = every slice
        bool overlap = false;
        double endIntervalSamples = 0.0; // top of the speed curve; 0 = no speed change
        bool pitchFollow = false;
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
        engine->setPingPongWidth (settings.pingPongWidth);
        engine->setEndIntervalSamples (settings.endIntervalSamples);
        engine->setPitchFollow (settings.pitchFollow);
        engine->setIntervalSamples (settings.intervalSamples);
        engine->setOverlap (settings.overlap);
        engine->prepare (kSampleRate, kBlockSize, settings.numChannels, kMaxSliceSeconds);
        return engine;
    }

    void processInBlocks (RepeaterEngine& engine, juce::AudioBuffer<float>& buffer, const std::vector<float>& curve = flatCurve, const std::vector<float>& speedCurve = {})
    {
        for (int start = 0; start < buffer.getNumSamples(); start += kBlockSize)
        {
            const auto length = juce::jmin (kBlockSize, buffer.getNumSamples() - start);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);
            engine.process (block, curve, speedCurve);
        }
    }

    // Processes [start, end) of the buffer in host-sized blocks, so a test can act between blocks
    void processRange (RepeaterEngine& engine, juce::AudioBuffer<float>& buffer, int start, int end)
    {
        for (int blockStart = start; blockStart < end; blockStart += kBlockSize)
        {
            const auto length = juce::jmin (kBlockSize, end - blockStart);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), blockStart, length);
            engine.process (block, flatCurve);
        }
    }

    std::vector<float> makeFlatSpeed (float value) { return std::vector<float> (2048, value); }

    void fillSine (juce::AudioBuffer<float>& buffer, int startSample, float frequency, float amplitude)
    {
        for (int i = startSample; i < buffer.getNumSamples(); ++i)
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample (channel, i, amplitude * std::sin (juce::MathConstants<float>::twoPi * frequency * (float) (i - startSample) / (float) kSampleRate));
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

TEST_CASE ("Ping-pong Width narrows the far side", "[RepeaterEngine]")
{
    const auto width = GENERATE (0.0f, 0.5f);
    auto engine = makeEngine ({ .pingPong = true, .pingPongWidth = width });
    auto buffer = makeImpulse (2, repeatSample (3), 0);

    processInBlocks (*engine, buffer);

    // Repeat 1 is on the left; the right gets the mono sum (0.5) scaled by 1 - width
    CHECK_THAT (buffer.getSample (0, repeatSample (1)), WithinAbs (0.5, 1.0e-6));
    CHECK_THAT (buffer.getSample (1, repeatSample (1)), WithinAbs (0.5 * (1.0 - width), 1.0e-6));
    CHECK_THAT (buffer.getSample (0, repeatSample (2)), WithinAbs (0.5 * (1.0 - width), 1.0e-6));
    CHECK_THAT (buffer.getSample (1, repeatSample (2)), WithinAbs (0.5, 1.0e-6));
}

TEST_CASE ("Speeding up without Pitch Follow keeps the pitch and cuts each repeat", "[RepeaterEngine]")
{
    constexpr int kHalfSlice = kSlice / 2;
    constexpr int kLateOffset = 3000; // beyond the sped-up interval, inside the slice
    auto engine = makeEngine ({ .endIntervalSamples = kHalfSlice });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeImpulse (2, origin + 6 * kHalfSlice);
    buffer.setSample (0, origin + kLateOffset, 1.0f); // during capture, so it doesn't retrigger

    processInBlocks (*engine, buffer, flatCurve, makeFlatSpeed (1.0f)); // top: half the slice

    for (int k = 1; k <= 4; ++k)
    {
        const auto start = origin + k * kHalfSlice;
        CHECK_THAT (buffer.getSample (0, start + preRoll), WithinAbs (1.0, 1.0e-6));
        CHECK_THAT (maxAbsInRange (buffer, 0, start + preRoll + 1, start + kHalfSlice), WithinAbs (0.0, 1.0e-6));
    }
}

TEST_CASE ("Pitch Follow at twice the speed plays each repeat an octave up", "[RepeaterEngine]")
{
    constexpr int kHalfSlice = kSlice / 2;
    auto engine = makeEngine ({ .endIntervalSamples = kHalfSlice, .pitchFollow = true });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeImpulse (2, origin + 6 * kHalfSlice);

    processInBlocks (*engine, buffer, flatCurve, makeFlatSpeed (1.0f));

    // The hit sits preRoll samples into the slice, so at double speed it comes preRoll / 2 in,
    // still inside the repeat's fade-in
    const auto fadeInGain = (double) (preRoll / 2) / (double) engine->getEdgeFadeSamples();
    for (int k = 1; k <= 4; ++k)
    {
        const auto start = origin + k * kHalfSlice;
        CHECK_THAT (buffer.getSample (0, start + preRoll / 2), WithinAbs (fadeInGain, 1.0e-6));
        CHECK_THAT (maxAbsInRange (buffer, 0, start + preRoll / 2 + 3, start + kHalfSlice), WithinAbs (0.0, 1.0e-6));
    }
}

TEST_CASE ("Slowing down with Pitch Follow plays at half speed", "[RepeaterEngine]")
{
    auto engine = makeEngine ({ .endIntervalSamples = 2 * kSlice, .pitchFollow = true });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeImpulse (2, origin + 5 * kSlice);

    processInBlocks (*engine, buffer, flatCurve, makeFlatSpeed (1.0f));

    const auto firstStart = origin + 2 * kSlice;
    CHECK_THAT (buffer.getSample (0, firstStart + 2 * preRoll), WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (buffer.getSample (0, firstStart + preRoll), WithinAbs (0.0, 1.0e-6));
}

TEST_CASE ("The speed curve steps the interval on a square-root scale", "[RepeaterEngine]")
{
    // Halfway up from S to S/2: ((sqrt 4800 + sqrt 2400) / 2)^2 = 3497.1, between the 3394 of a
    // log scale and the 3600 of equal milliseconds
    constexpr int kInterval = 3497;
    auto engine = makeEngine ({ .endIntervalSamples = kSlice / 2 });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeImpulse (2, origin + 6 * kInterval);

    processInBlocks (*engine, buffer, flatCurve, makeFlatSpeed (0.5f));

    for (int k = 1; k <= 4; ++k)
    {
        CHECK_THAT (buffer.getSample (0, origin + k * kInterval + preRoll), WithinAbs (1.0, 1.0e-6));
        CHECK_THAT (maxAbsInRange (buffer, 0, origin + k * kInterval + preRoll + 1, origin + (k + 1) * kInterval), WithinAbs (0.0, 1.0e-6));
    }
}

TEST_CASE ("Each repeat's interval comes from the speed curve where it starts", "[RepeaterEngine]")
{
    // Bottom until t = 0.586, then the top (half the slice)
    std::vector<float> stepSpeed (2048, 0.0f);
    std::fill (stepSpeed.begin() + 1200, stepSpeed.end(), 1.0f);

    auto engine = makeEngine ({ .endIntervalSamples = kSlice / 2 });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeImpulse (2, origin + kLength);
    processInBlocks (*engine, buffer, flatCurve, stepSpeed);

    // Repeat 5 starts at t = 0.5 (bottom: next one S later); repeat 6 at t = 0.6 (top: S/2 later)
    CHECK_THAT (buffer.getSample (0, origin + 5 * kSlice + preRoll), WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (buffer.getSample (0, origin + 5 * kSlice + kSlice / 2 + preRoll), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (buffer.getSample (0, origin + 6 * kSlice + preRoll), WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (buffer.getSample (0, origin + 6 * kSlice + kSlice / 2 + preRoll), WithinAbs (1.0, 1.0e-6));
}

TEST_CASE ("Audio-rate repeats don't click", "[RepeaterEngine]")
{
    // A slow rise from -34 dB to -26 dB crosses the -30 dB threshold without a step, so the slice
    // holds smooth audio around the hit; a hard cut would step by the whole level
    constexpr int kMaxSpeedInterval = 240; // 5 ms
    constexpr float kQuiet = 0.02f;
    constexpr float kLoud = 0.05f;
    constexpr int kRiseSamples = 1000;
    auto engine = makeEngine ({ .endIntervalSamples = kMaxSpeedInterval });
    auto buffer = makeSilence (2, kHitSample + kLength);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto rise = juce::jlimit (0.0f, 1.0f, (float) (i - kHitSample + kRiseSamples / 2) / (float) kRiseSamples);
        for (int channel = 0; channel < 2; ++channel)
            buffer.setSample (channel, i, kQuiet + (kLoud - kQuiet) * rise);
    }

    processInBlocks (*engine, buffer, flatCurve, makeFlatSpeed (1.0f));

    // Fades are a quarter of the interval; the fade-out at the end of Length and the rise itself
    // can add their slopes on top
    const auto bumpStep = kLoud / (float) (kMaxSpeedInterval / 4);
    const auto endFadeStep = kLoud / (float) engine->getEdgeFadeSamples();
    const auto riseStep = (kLoud - kQuiet) / (float) kRiseSamples;
    CHECK (maxAbsInRange (buffer, 0, 0, buffer.getNumSamples()) > kQuiet);
    CHECK (maxSampleStep (buffer, 0) < 1.05f * (bumpStep + endFadeStep + riseStep));
}

TEST_CASE ("Repeats shorter than the pre-roll start one fade-in before the hit", "[RepeaterEngine]")
{
    // 200 samples apart is less than the 240-sample pre-roll: from the slice start these repeats
    // would only replay the silence before the hit
    constexpr int kInterval = 200;
    auto engine = makeEngine ({ .endIntervalSamples = kInterval });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeSilence (2, origin + 10 * kInterval);
    for (int channel = 0; channel < 2; ++channel)
        juce::FloatVectorOperations::fill (buffer.getWritePointer (channel, kHitSample), 0.5f, buffer.getNumSamples() - kHitSample);

    processInBlocks (*engine, buffer, flatCurve, makeFlatSpeed (1.0f));

    // The first start inside the pre-roll (200) is skipped. From 400 on each repeat reads from
    // 50 samples before the hit, its fade-in length, so the hit arrives at full gain
    constexpr int kFade = kInterval / 4;
    for (int k = 2; k <= 4; ++k)
    {
        const auto start = origin + k * kInterval;
        CHECK_THAT (buffer.getSample (0, start + kFade / 2), WithinAbs (0.0, 1.0e-6));
        CHECK_THAT (buffer.getSample (0, start + kFade), WithinAbs (0.5, 1.0e-6));
    }
}

TEST_CASE ("Short repeats keep a sixth of their length of pre-roll", "[RepeaterEngine]")
{
    // 1200 samples apart (25 ms): a sixth is 200 of the 240 pre-roll samples, so each repeat
    // starts 40 samples into the slice and the hit lands 200 samples in
    constexpr int kInterval = 1200;
    auto engine = makeEngine ({ .endIntervalSamples = kInterval });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeImpulse (2, origin + 6 * kInterval);

    processInBlocks (*engine, buffer, flatCurve, makeFlatSpeed (1.0f));

    for (int k = 1; k <= 4; ++k)
    {
        const auto start = origin + k * kInterval;
        CHECK_THAT (buffer.getSample (0, start + kInterval / 6), WithinAbs (1.0, 1.0e-6));
        CHECK_THAT (buffer.getSample (0, start + preRoll), WithinAbs (0.0, 1.0e-6));
    }
}

TEST_CASE ("Sped-up repeats with Pitch Follow fade at their edges", "[RepeaterEngine]")
{
    auto engine = makeEngine ({ .endIntervalSamples = kSlice / 2, .pitchFollow = true });
    auto buffer = makeSilence (2, kHitSample + kLength);
    constexpr float kFrequency = 440.0f;
    constexpr float kAmplitude = 0.5f;
    fillSine (buffer, kHitSample, kFrequency, kAmplitude);

    processInBlocks (*engine, buffer, flatCurve, makeFlatSpeed (1.0f));

    // An octave up doubles the sine's slope; the fades add at most one fade step
    const auto naturalStep = 2.0f * juce::MathConstants<float>::twoPi * kFrequency / (float) kSampleRate * kAmplitude;
    const auto fadeStep = kAmplitude / (float) engine->getEdgeFadeSamples();
    CHECK (maxAbsInRange (buffer, 0, 0, buffer.getNumSamples()) > 0.4f);
    CHECK (maxSampleStep (buffer, 0) < naturalStep + fadeStep + 1.0e-3f);
}

TEST_CASE ("Accelerating overlapped repeats fade out instead of being cut", "[RepeaterEngine]")
{
    // From 1200 down to 100 samples apart: the 16th next repeat comes long before 16 of a
    // repeat's own intervals, so its slot is reused while it still has audio left
    std::vector<float> rampUp (2048);
    for (size_t i = 0; i < rampUp.size(); ++i)
        rampUp[i] = (float) i / (float) (rampUp.size() - 1);

    auto engine = makeEngine ({ .intervalSamples = 1200, .overlap = true, .endIntervalSamples = 100 });
    auto buffer = makeSilence (2, kHitSample + kLength);
    addSmoothBurst (buffer, kHitSample - 32, kLength - 1000); // a plateau through the capture

    processInBlocks (*engine, buffer, flatCurve, rampUp);

    CHECK (maxAbsInRange (buffer, 0, 0, buffer.getNumSamples()) > 4.0f); // many repeats stacked
    CHECK (maxSampleStep (buffer, 0) < 0.1f);
}

TEST_CASE ("An interval longer than the slice leaves gaps", "[RepeaterEngine]")
{
    constexpr int kInterval = 2 * kSlice;
    auto engine = makeEngine ({ .intervalSamples = kInterval });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeImpulse (2, origin + kLength + 1000);

    processInBlocks (*engine, buffer);

    for (int k = 1; k * kInterval < kLength; ++k)
    {
        const auto start = origin + k * kInterval;
        CHECK_THAT (buffer.getSample (0, start + preRoll), WithinAbs (1.0, 1.0e-6));
        CHECK_THAT (maxAbsInRange (buffer, 0, start + preRoll + 1, start + kInterval), WithinAbs (0.0, 1.0e-6));
    }
}

TEST_CASE ("A short interval cuts each repeat, or lets it ring with Overlap", "[RepeaterEngine]")
{
    constexpr int kInterval = 1200;
    constexpr int kLateOffset = 3000; // beyond the interval, inside the slice
    const auto overlap = GENERATE (false, true);

    auto engine = makeEngine ({ .intervalSamples = kInterval, .overlap = overlap });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeImpulse (2, origin + kLength);
    buffer.setSample (0, origin + kLateOffset, 1.0f); // during capture, so it doesn't retrigger

    processInBlocks (*engine, buffer);

    // Repeat 1 starts at the first interval after the pre-roll. Cut at 25 ms it keeps only a
    // sixth of its length of pre-roll (200 of 240 samples); ringing with Overlap it keeps it all.
    const auto firstStart = origin + kInterval;
    const auto skippedPreRoll = overlap ? 0 : preRoll - kInterval / 6;
    CHECK_THAT (buffer.getSample (0, firstStart + preRoll - skippedPreRoll), WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (buffer.getSample (0, firstStart + kLateOffset - skippedPreRoll), WithinAbs (overlap ? 1.0 : 0.0, 1.0e-6));
}

TEST_CASE ("Overlap plays at most 16 repeats at once", "[RepeaterEngine]")
{
    constexpr int kInterval = 100; // 16 repeats span 1600 samples
    constexpr int kLateOffset = 2000;
    auto engine = makeEngine ({ .intervalSamples = kInterval, .overlap = true });
    const auto preRoll = engine->getPreRollSamples();
    const auto origin = kHitSample - preRoll;
    auto buffer = makeImpulse (2, origin + kLength);
    buffer.setSample (0, origin + kLateOffset, 1.0f);

    processInBlocks (*engine, buffer);

    // Each repeat is cut before it reaches the late impulse, so only the hit is repeated:
    // copies of it land 40 samples past every 100-sample grid line, nothing lands on them
    float onGrid = 0.0f;
    float hitCopies = 0.0f;
    for (int t = origin + 3 * kInterval; t < origin + kLength - kInterval; ++t)
    {
        const auto value = std::abs (buffer.getSample (0, t));
        if ((t - origin) % kInterval == 0)
            onGrid = juce::jmax (onGrid, value);
        if ((t - origin) % kInterval == preRoll % kInterval)
            hitCopies = juce::jmax (hitCopies, value);
    }

    CHECK_THAT (onGrid, WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (hitCopies, WithinAbs (1.0, 1.0e-6));
}

TEST_CASE ("A sped-up repeat inside the capture fades out before catching up", "[RepeaterEngine]")
{
    constexpr int kInterval = 1200;
    auto engine = makeEngine ({ .intervalSamples = kInterval, .overlap = true, .endIntervalSamples = kInterval / 2, .pitchFollow = true });
    auto buffer = makeSilence (2, kHitSample + kLength);
    addSmoothBurst (buffer, kHitSample - 32, kSlice + 2000); // a plateau through the capture

    processInBlocks (*engine, buffer, flatCurve, makeFlatSpeed (1.0f)); // 600 samples apart, double speed

    // Reading past the capture would jump to stale audio; the fades keep every step small
    CHECK (maxAbsInRange (buffer, 0, 0, buffer.getNumSamples()) > 0.4f);
    CHECK (maxSampleStep (buffer, 0) < 0.05f);
}

TEST_CASE ("Stop fades the repeats out, and the next hit starts a new tail", "[RepeaterEngine]")
{
    auto engine = makeEngine ({});
    const auto secondHit = kHitSample + kLength / 2;
    auto buffer = makeSilence (2, kHitSample + kLength);
    addSmoothBurst (buffer, kHitSample - 32, kSlice);
    addSmoothBurst (buffer, secondHit - 32, kSlice);

    // Stop mid-plateau of repeat 3, on a block boundary
    const auto stopSample = 34 * kBlockSize; // 17408, about 2000 samples into repeat 3
    REQUIRE (stopSample > kHitSample + 3 * kSlice);
    processRange (*engine, buffer, 0, stopSample);
    REQUIRE (engine->getPlayheadPosition() > 0.0f);

    engine->stop();
    processRange (*engine, buffer, stopSample, buffer.getNumSamples());

    // A hard cut would step by 0.5; the 3 ms fade keeps every step small
    CHECK (maxSampleStep (buffer, 0) < 0.01f);
    CHECK (std::abs (buffer.getSample (0, stopSample - 1)) > 0.4f);
    CHECK_THAT (maxAbsInRange (buffer, 0, stopSample + 150, secondHit), WithinAbs (0.0, 1.0e-6));

    // The second hit repeats as normal
    CHECK_THAT (buffer.getSample (0, secondHit + kSlice + kSlice / 2), WithinAbs (0.5, 1.0e-3));
}
