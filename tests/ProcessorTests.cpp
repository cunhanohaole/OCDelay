#include <Parameters.h>
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
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

    juce::AudioProcessor::BusesLayout makeLayout (const juce::AudioChannelSet& input, const juce::AudioChannelSet& output)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (input);
        layout.outputBuses.add (output);
        return layout;
    }
}

TEST_CASE ("Parameters have PRD defaults", "[processor]")
{
    PluginProcessor plugin;
    using Catch::Matchers::WithinAbs;

    CHECK_THAT (getPlainValue (plugin, ParamIDs::delayTime), WithinAbs (500.0, 0.01));
    CHECK_THAT (getPlainValue (plugin, ParamIDs::feedback), WithinAbs (35.0, 0.01));
    CHECK_THAT (getPlainValue (plugin, ParamIDs::mix), WithinAbs (50.0, 0.01));
    CHECK (getPlainValue (plugin, ParamIDs::pingPong) < 0.5f);
}

TEST_CASE ("Parameter ranges match PRD", "[processor]")
{
    PluginProcessor plugin;
    auto& apvts = plugin.getAPVTS();

    const auto delayRange = apvts.getParameterRange (ParamIDs::delayTime.getParamID());
    CHECK (delayRange.start == 1.0f);
    CHECK (delayRange.end == 2000.0f);

    CHECK (apvts.getParameterRange (ParamIDs::feedback.getParamID()).end == 95.0f);
    CHECK (apvts.getParameterRange (ParamIDs::mix.getParamID()).end == 100.0f);
}

TEST_CASE ("Parameter text shows units", "[processor]")
{
    PluginProcessor plugin;
    auto* delay = plugin.getAPVTS().getParameter (ParamIDs::delayTime.getParamID());
    auto* feedback = plugin.getAPVTS().getParameter (ParamIDs::feedback.getParamID());

    CHECK (delay->getCurrentValueAsText() == "500 ms");
    CHECK (feedback->getCurrentValueAsText() == "35 %");
}

TEST_CASE ("State round-trips through get/setStateInformation", "[processor]")
{
    PluginProcessor source;
    setPlainValue (source, ParamIDs::delayTime, 1234.0f);
    setPlainValue (source, ParamIDs::feedback, 80.0f);
    setPlainValue (source, ParamIDs::mix, 25.0f);
    setPlainValue (source, ParamIDs::pingPong, 1.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());

    using Catch::Matchers::WithinAbs;
    CHECK_THAT (getPlainValue (restored, ParamIDs::delayTime), WithinAbs (1234.0, 0.5));
    CHECK_THAT (getPlainValue (restored, ParamIDs::feedback), WithinAbs (80.0, 0.01));
    CHECK_THAT (getPlainValue (restored, ParamIDs::mix), WithinAbs (25.0, 0.01));
    CHECK (getPlainValue (restored, ParamIDs::pingPong) > 0.5f);
}

TEST_CASE ("Garbage state is ignored", "[processor]")
{
    PluginProcessor plugin;
    const char garbage[] = "not a valid state";
    plugin.setStateInformation (garbage, (int) sizeof (garbage));

    CHECK_THAT (getPlainValue (plugin, ParamIDs::delayTime), Catch::Matchers::WithinAbs (500.0, 0.01));
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

namespace
{
    // Feeds an impulse on input channel 0 through processBlock and returns the output
    juce::AudioBuffer<float> processImpulse (PluginProcessor& plugin, const juce::AudioChannelSet& input, const juce::AudioChannelSet& output)
    {
        constexpr double sampleRate = 48000.0;
        constexpr int numSamples = 48000;

        REQUIRE (plugin.setBusesLayout (makeLayout (input, output)));
        setPlainValue (plugin, ParamIDs::delayTime, 10.0f);
        setPlainValue (plugin, ParamIDs::feedback, 0.0f);
        setPlainValue (plugin, ParamIDs::mix, 100.0f);
        plugin.prepareToPlay (sampleRate, numSamples);

        juce::AudioBuffer<float> buffer (output.size(), numSamples);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);

        juce::MidiBuffer midi;
        plugin.processBlock (buffer, midi);
        return buffer;
    }
}

TEST_CASE ("Mono input on a stereo output delays both sides", "[processor]")
{
    PluginProcessor plugin;
    auto output = processImpulse (plugin, juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo());

    CHECK_THAT (output.getSample (0, 480), Catch::Matchers::WithinAbs (1.0, 1.0e-4));
    CHECK_THAT (output.getSample (1, 480), Catch::Matchers::WithinAbs (1.0, 1.0e-4));
}

TEST_CASE ("Mono to mono layout processes a single channel", "[processor]")
{
    PluginProcessor plugin;
    auto output = processImpulse (plugin, juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono());

    REQUIRE (output.getNumChannels() == 1);
    CHECK_THAT (output.getSample (0, 480), Catch::Matchers::WithinAbs (1.0, 1.0e-4));
}

TEST_CASE ("Tail length covers the decaying repeats", "[processor]")
{
    PluginProcessor plugin;

    setPlainValue (plugin, ParamIDs::delayTime, 500.0f);
    setPlainValue (plugin, ParamIDs::feedback, 0.0f);
    CHECK_THAT (plugin.getTailLengthSeconds(), Catch::Matchers::WithinAbs (0.5, 0.01));

    // 50% feedback needs 10 repeats to fall 60 dB, plus the first repeat
    setPlainValue (plugin, ParamIDs::feedback, 50.0f);
    CHECK_THAT (plugin.getTailLengthSeconds(), Catch::Matchers::WithinAbs (0.5 * 11.0, 0.01));
}
