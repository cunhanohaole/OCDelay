#include <Parameters.h>
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    // With no playhead the processor uses 120 BPM in 4/4, and the pre-roll is 5 ms
    constexpr int kPreRollSamples = 240;

    float getPlainValue (PluginProcessor& plugin, const juce::ParameterID& id)
    {
        return plugin.getAPVTS().getRawParameterValue (id.getParamID())->load();
    }

    void setPlainValue (PluginProcessor& plugin, const juce::ParameterID& id, float plainValue)
    {
        auto* parameter = plugin.getAPVTS().getParameter (id.getParamID());
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    juce::String getText (PluginProcessor& plugin, const juce::ParameterID& id)
    {
        return plugin.getAPVTS().getParameter (id.getParamID())->getCurrentValueAsText();
    }

    juce::AudioProcessor::BusesLayout makeLayout (const juce::AudioChannelSet& input, const juce::AudioChannelSet& output)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (input);
        layout.outputBuses.add (output);
        return layout;
    }

    void useFlatCurve (PluginProcessor& plugin)
    {
        EnvelopeCurve flat;
        flat.movePoint (1, 1.0f, 1.0f);
        plugin.updateCurve (flat);
    }

    void processInBlocks (PluginProcessor& plugin, juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer midi;

        for (int start = 0; start < buffer.getNumSamples(); start += kBlockSize)
        {
            const auto length = juce::jmin (kBlockSize, buffer.getNumSamples() - start);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);
            plugin.processBlock (block, midi);
        }
    }

    // Silence, then a 100-sample burst of 0.5 on the input channels starting at hitSample,
    // processed 100% wet in host-sized blocks
    juce::AudioBuffer<float> processHit (PluginProcessor& plugin,
        const juce::AudioChannelSet& input,
        const juce::AudioChannelSet& output,
        int numSamples,
        int hitSample = 1000)
    {
        REQUIRE (plugin.setBusesLayout (makeLayout (input, output)));
        setPlainValue (plugin, ParamIDs::mix, 100.0f);
        plugin.prepareToPlay (kSampleRate, kBlockSize);

        juce::AudioBuffer<float> buffer (output.size(), numSamples);
        buffer.clear();
        for (int channel = 0; channel < input.size(); ++channel)
            for (int i = hitSample; i < hitSample + 100; ++i)
                buffer.setSample (channel, i, 0.5f);

        processInBlocks (plugin, buffer);
        return buffer;
    }

    juce::AudioBuffer<float> processStereoHit (PluginProcessor& plugin, int numSamples, int hitSample = 1000)
    {
        const auto stereo = juce::AudioChannelSet::stereo();
        return processHit (plugin, stereo, stereo, numSamples, hitSample);
    }
}

//==============================================================================
TEST_CASE ("Parameters have PRD defaults", "[processor]")
{
    PluginProcessor plugin;

    CHECK_THAT (getPlainValue (plugin, ParamIDs::delayTime), WithinAbs (500.0, 0.01));
    CHECK_THAT (getPlainValue (plugin, ParamIDs::mix), WithinAbs (50.0, 0.01));
    CHECK_THAT (getPlainValue (plugin, ParamIDs::threshold), WithinAbs (-30.0, 0.01));
    CHECK (getPlainValue (plugin, ParamIDs::pingPong) < 0.5f);
    CHECK (getPlainValue (plugin, ParamIDs::sync) < 0.5f);
    CHECK (getText (plugin, ParamIDs::division) == "1/4");
    CHECK (getText (plugin, ParamIDs::length) == "8");
    CHECK (plugin.getLengthBars() == 8);
    CHECK_THAT (getPlainValue (plugin, ParamIDs::pingPongWidth), WithinAbs (100.0, 0.01));
    CHECK_THAT (getPlainValue (plugin, ParamIDs::pitchRange), WithinAbs (12.0, 0.01));
    CHECK (getPlainValue (plugin, ParamIDs::pitchBipolar) < 0.5f);
    CHECK (plugin.getCurve (CurveTarget::pitch) == EnvelopeCurve::makeFlat (0.0f));
    CHECK (getPlainValue (plugin, ParamIDs::freeRate) < 0.5f);
    CHECK (getPlainValue (plugin, ParamIDs::rateOverlap) < 0.5f);
    CHECK_THAT (getPlainValue (plugin, ParamIDs::rate), WithinAbs (500.0, 0.01));
    CHECK (getText (plugin, ParamIDs::rateDivision) == "1/4");
}

TEST_CASE ("Length offers 1 to 32 bars", "[processor]")
{
    PluginProcessor plugin;
    const std::array expected { 1, 2, 4, 8, 16, 32 };

    for (size_t i = 0; i < expected.size(); ++i)
    {
        setPlainValue (plugin, ParamIDs::length, (float) i);
        CHECK (plugin.getLengthBars() == expected[i]);
    }
}

TEST_CASE ("Feedback is retired", "[processor]")
{
    PluginProcessor plugin;
    CHECK (plugin.getAPVTS().getParameter ("feedback") == nullptr);
}

TEST_CASE ("Parameter ranges match PRD", "[processor]")
{
    PluginProcessor plugin;
    auto& apvts = plugin.getAPVTS();

    CHECK (juce::exactlyEqual (apvts.getParameterRange (ParamIDs::delayTime.getParamID()).start, 1.0f));
    CHECK (juce::exactlyEqual (apvts.getParameterRange (ParamIDs::delayTime.getParamID()).end, 2000.0f));
    CHECK (juce::exactlyEqual (apvts.getParameterRange (ParamIDs::mix.getParamID()).end, 100.0f));
    CHECK (juce::exactlyEqual (apvts.getParameterRange (ParamIDs::threshold.getParamID()).start, -60.0f));
    CHECK (juce::exactlyEqual (apvts.getParameterRange (ParamIDs::threshold.getParamID()).end, 0.0f));
    CHECK (juce::exactlyEqual (apvts.getParameterRange (ParamIDs::pingPongWidth.getParamID()).end, 100.0f));
    CHECK (juce::exactlyEqual (apvts.getParameterRange (ParamIDs::pitchRange.getParamID()).start, 1.0f));
    CHECK (juce::exactlyEqual (apvts.getParameterRange (ParamIDs::pitchRange.getParamID()).end, 48.0f));
}

TEST_CASE ("Parameter text shows units", "[processor]")
{
    PluginProcessor plugin;

    CHECK (getText (plugin, ParamIDs::delayTime) == "500 ms");
    CHECK (getText (plugin, ParamIDs::mix) == "50 %");
    CHECK (getText (plugin, ParamIDs::threshold) == "-30 dB");
    CHECK (getText (plugin, ParamIDs::pingPongWidth) == "100 %");
    CHECK (getText (plugin, ParamIDs::pitchRange) == "12 st");
}

TEST_CASE ("Supported bus layouts", "[processor]")
{
    PluginProcessor plugin;
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    CHECK (plugin.checkBusesLayoutSupported (makeLayout (mono, mono)));
    CHECK (plugin.checkBusesLayoutSupported (makeLayout (mono, stereo)));
    CHECK (plugin.checkBusesLayoutSupported (makeLayout (stereo, stereo)));

    CHECK_FALSE (plugin.checkBusesLayoutSupported (makeLayout (stereo, mono)));
    CHECK_FALSE (plugin.checkBusesLayoutSupported (makeLayout (juce::AudioChannelSet::createLCR(), stereo)));
    CHECK_FALSE (plugin.checkBusesLayoutSupported (makeLayout (stereo, juce::AudioChannelSet::create5point1())));
}

//==============================================================================
TEST_CASE ("State round-trips parameters and the curve", "[processor]")
{
    PluginProcessor source;
    setPlainValue (source, ParamIDs::delayTime, 1234.0f);
    setPlainValue (source, ParamIDs::threshold, -42.0f);
    setPlainValue (source, ParamIDs::length, 5.0f); // 32 bars
    setPlainValue (source, ParamIDs::pingPong, 1.0f);

    auto edited = source.getCurve();
    edited.addPoint (0.5f, 1.0f);
    edited.setCurvature (1, 0.5f);
    source.updateCurve (edited);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    PluginProcessor restored;
    const auto versionBefore = restored.getCurveVersion();
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK_THAT (getPlainValue (restored, ParamIDs::delayTime), WithinAbs (1234.0, 0.5));
    CHECK_THAT (getPlainValue (restored, ParamIDs::threshold), WithinAbs (-42.0, 0.01));
    CHECK (restored.getLengthBars() == 32);
    CHECK (getPlainValue (restored, ParamIDs::pingPong) > 0.5f);
    CHECK (restored.getCurve() == edited);
    CHECK (restored.getCurveVersion() > versionBefore);
}

TEST_CASE ("State keeps the volume and pitch curves apart", "[processor]")
{
    PluginProcessor source;
    setPlainValue (source, ParamIDs::pitchRange, 24.0f);
    setPlainValue (source, ParamIDs::pitchBipolar, 1.0f);
    setPlainValue (source, ParamIDs::pingPongWidth, 40.0f);

    auto pitch = EnvelopeCurve::makeFlat (0.5f);
    pitch.addPoint (0.25f, 0.9f);
    source.updateCurve (pitch, CurveTarget::pitch);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.getCurve (CurveTarget::pitch) == pitch);
    CHECK (restored.getCurve (CurveTarget::volume) == EnvelopeCurve {});
    CHECK_THAT (getPlainValue (restored, ParamIDs::pitchRange), WithinAbs (24.0, 0.01));
    CHECK (restored.isPitchBipolar());
    CHECK_THAT (getPlainValue (restored, ParamIDs::pingPongWidth), WithinAbs (40.0, 0.01));
}

TEST_CASE ("A v0.1 session loads cleanly", "[processor]")
{
    // v0.1 saved Feedback and had no curve
    juce::XmlElement xml ("OCDelayState");
    for (const auto& [id, value] : { std::pair { "delayTime", 1234.0 }, std::pair { "feedback", 80.0 }, std::pair { "mix", 25.0 } })
    {
        auto* param = xml.createNewChildElement ("PARAM");
        param->setAttribute ("id", id);
        param->setAttribute ("value", value);
    }

    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary (xml, state);

    PluginProcessor restored;
    auto edited = restored.getCurve();
    edited.addPoint (0.5f, 0.2f);
    restored.updateCurve (edited);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK_THAT (getPlainValue (restored, ParamIDs::delayTime), WithinAbs (1234.0, 0.5));
    CHECK_THAT (getPlainValue (restored, ParamIDs::mix), WithinAbs (25.0, 0.01));
    CHECK (restored.getCurve() == EnvelopeCurve {});
    CHECK (restored.getCurve (CurveTarget::pitch) == EnvelopeCurve::makeFlat (0.0f));
}

TEST_CASE ("Garbage state is ignored", "[processor]")
{
    PluginProcessor plugin;
    const char garbage[] = "not a valid state";
    plugin.setStateInformation (garbage, (int) sizeof (garbage));

    CHECK_THAT (getPlainValue (plugin, ParamIDs::delayTime), WithinAbs (500.0, 0.01));
}

//==============================================================================
TEST_CASE ("A hit repeats at the slice length", "[processor]")
{
    PluginProcessor plugin;
    useFlatCurve (plugin);
    setPlainValue (plugin, ParamIDs::delayTime, 250.0f); // 12000 samples

    const auto output = processStereoHit (plugin, 40000);

    // 100% wet: the original is silent, then each repeat replays the burst one slice later
    CHECK_THAT (output.getSample (0, 1050), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (output.getSample (0, 13050), WithinAbs (0.5, 1.0e-4));
    CHECK_THAT (output.getSample (1, 25050), WithinAbs (0.5, 1.0e-4));
}

TEST_CASE ("Free Rate starts repeats every Rate instead of every Slice", "[processor]")
{
    PluginProcessor plugin;
    useFlatCurve (plugin);
    setPlainValue (plugin, ParamIDs::delayTime, 125.0f); // 6000-sample slice
    setPlainValue (plugin, ParamIDs::rate, 250.0f); // 12000 samples

    SECTION ("off: Rate is ignored")
    {
        const auto output = processStereoHit (plugin, 40000);
        CHECK_THAT (output.getSample (0, 7050), WithinAbs (0.5, 1.0e-4));
        CHECK_THAT (plugin.getRepeatIntervalMs(), WithinAbs (125.0, 0.01));
    }

    SECTION ("on: repeats every 250 ms, silent in between")
    {
        setPlainValue (plugin, ParamIDs::freeRate, 1.0f);
        const auto output = processStereoHit (plugin, 40000);
        CHECK_THAT (output.getSample (0, 7050), WithinAbs (0.0, 1.0e-6));
        CHECK_THAT (output.getSample (0, 13050), WithinAbs (0.5, 1.0e-4));
        CHECK_THAT (output.getSample (0, 25050), WithinAbs (0.5, 1.0e-4));
        CHECK_THAT (plugin.getRepeatIntervalMs(), WithinAbs (250.0, 0.01));
    }
}

TEST_CASE ("Synced Rate uses the Rate Division", "[processor]")
{
    PluginProcessor plugin;
    useFlatCurve (plugin);
    setPlainValue (plugin, ParamIDs::sync, 1.0f);
    setPlainValue (plugin, ParamIDs::freeRate, 1.0f);
    setPlainValue (plugin, ParamIDs::division, (float) TempoSync::getDivisionNames().indexOf ("1/16")); // 125 ms
    setPlainValue (plugin, ParamIDs::rateDivision, (float) TempoSync::getDivisionNames().indexOf ("1/8")); // 250 ms

    const auto output = processStereoHit (plugin, 40000);
    CHECK_THAT (output.getSample (0, 7050), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (output.getSample (0, 13050), WithinAbs (0.5, 1.0e-4));
}

TEST_CASE ("Synced slice uses the note division at the host tempo", "[processor]")
{
    PluginProcessor plugin;
    useFlatCurve (plugin);
    setPlainValue (plugin, ParamIDs::sync, 1.0f);
    setPlainValue (plugin, ParamIDs::division, (float) TempoSync::getDivisionNames().indexOf ("1/16")); // 125 ms

    const auto output = processStereoHit (plugin, 20000);

    CHECK_THAT (output.getSample (0, 7050), WithinAbs (0.5, 1.0e-4));
    CHECK_THAT (plugin.getSliceMs(), WithinAbs (125.0, 0.01));
}

TEST_CASE ("Repeats stop exactly at the end of Length", "[processor]")
{
    // 4 bars at 120 BPM = 8 s from the voice origin (the hit minus the pre-roll)
    PluginProcessor plugin;
    useFlatCurve (plugin);
    setPlainValue (plugin, ParamIDs::length, 2.0f); // 4 bars
    setPlainValue (plugin, ParamIDs::delayTime, 125.0f); // 6000 samples

    constexpr int hit = 1000;
    constexpr int lengthSamples = 8 * 48000;
    constexpr int end = hit - kPreRollSamples + lengthSamples;
    const auto output = processStereoHit (plugin, end + 20000, hit);

    // The last repeat starts one slice before the end; the next would start on the end
    CHECK_THAT (output.getSample (0, end - 6000 + kPreRollSamples + 50), WithinAbs (0.5, 1.0e-4));
    CHECK (juce::exactlyEqual (output.getMagnitude (0, end, output.getNumSamples() - end), 0.0f));
}

TEST_CASE ("Mono input on a stereo output repeats on both sides", "[processor]")
{
    PluginProcessor plugin;
    useFlatCurve (plugin);
    setPlainValue (plugin, ParamIDs::delayTime, 250.0f);

    const auto output = processHit (plugin, juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo(), 20000);

    CHECK_THAT (output.getSample (0, 13050), WithinAbs (0.5, 1.0e-4));
    CHECK_THAT (output.getSample (1, 13050), WithinAbs (0.5, 1.0e-4));
}

TEST_CASE ("Mono to mono layout repeats a single channel", "[processor]")
{
    PluginProcessor plugin;
    useFlatCurve (plugin);
    setPlainValue (plugin, ParamIDs::delayTime, 250.0f);

    const auto mono = juce::AudioChannelSet::mono();
    const auto output = processHit (plugin, mono, mono, 20000);

    REQUIRE (output.getNumChannels() == 1);
    CHECK_THAT (output.getSample (0, 13050), WithinAbs (0.5, 1.0e-4));
}

TEST_CASE ("Tail length is Length at the host tempo", "[processor]")
{
    PluginProcessor plugin;
    CHECK_THAT (plugin.getTailLengthSeconds(), WithinAbs (16.0, 1.0e-6)); // 8 bars at 120 BPM

    setPlainValue (plugin, ParamIDs::length, 5.0f);
    CHECK_THAT (plugin.getTailLengthSeconds(), WithinAbs (64.0, 1.0e-6)); // 32 bars
}

TEST_CASE ("Playhead reports progress through the tail", "[processor]")
{
    PluginProcessor plugin;
    CHECK (plugin.getPlayheadPosition() < 0.0f);

    processStereoHit (plugin, 48000);

    // About 1 s into an 8-bar (16 s) tail
    CHECK_THAT (plugin.getPlayheadPosition(), WithinAbs (1.0 / 16.0, 0.01));
}
