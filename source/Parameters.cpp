#include "Parameters.h"

namespace
{
    juce::String formatMilliseconds (float value, int /*maximumStringLength*/)
    {
        return value < 10.0f ? juce::String (value, 1) + " ms"
                             : juce::String (juce::roundToInt (value)) + " ms";
    }

    juce::String formatPercent (float value, int /*maximumStringLength*/)
    {
        return juce::String (juce::roundToInt (value)) + " %";
    }

    juce::String formatSemitones (float value, int /*maximumStringLength*/)
    {
        return juce::String (juce::roundToInt (value)) + " st";
    }

    juce::String formatDecibels (float value, int /*maximumStringLength*/)
    {
        return juce::String (juce::roundToInt (value)) + " dB";
    }

    // Accepts "500", "500 ms" or "35 %" when the user types into a value box
    float parseLeadingNumber (const juce::String& text)
    {
        return text.retainCharacters ("0123456789.-").getFloatValue();
    }

    std::unique_ptr<juce::AudioParameterFloat> makeDelayTimeParameter()
    {
        juce::NormalisableRange<float> range { ParamRanges::delayTimeMinMs, ParamRanges::delayTimeMaxMs };
        range.setSkewForCentre (ParamRanges::delayTimeSkewCentreMs);

        return std::make_unique<juce::AudioParameterFloat> (
            ParamIDs::delayTime,
            "Slice",
            range,
            ParamRanges::delayTimeDefaultMs,
            juce::AudioParameterFloatAttributes()
                .withLabel ("ms")
                .withStringFromValueFunction (formatMilliseconds)
                .withValueFromStringFunction (parseLeadingNumber));
    }

    std::unique_ptr<juce::AudioParameterFloat> makePercentParameter (const juce::ParameterID& id,
        const juce::String& name,
        float maxPercent,
        float defaultPercent)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            id,
            name,
            juce::NormalisableRange<float> { 0.0f, maxPercent },
            defaultPercent,
            juce::AudioParameterFloatAttributes()
                .withLabel ("%")
                .withStringFromValueFunction (formatPercent)
                .withValueFromStringFunction (parseLeadingNumber));
    }
}

namespace
{
    std::unique_ptr<juce::AudioParameterChoice> makeLengthParameter()
    {
        juce::StringArray choices;
        for (const auto bars : ParamRanges::lengthBarsOptions)
            choices.add (juce::String (bars));

        return std::make_unique<juce::AudioParameterChoice> (
            ParamIDs::length,
            "Length",
            choices,
            ParamRanges::lengthDefaultIndex,
            juce::AudioParameterChoiceAttributes().withLabel ("bars"));
    }

    std::unique_ptr<juce::AudioParameterFloat> makeThresholdParameter()
    {
        return std::make_unique<juce::AudioParameterFloat> (
            ParamIDs::threshold,
            "Threshold",
            juce::NormalisableRange<float> { ParamRanges::thresholdMinDb, ParamRanges::thresholdMaxDb },
            ParamRanges::thresholdDefaultDb,
            juce::AudioParameterFloatAttributes()
                .withLabel ("dB")
                .withStringFromValueFunction (formatDecibels)
                .withValueFromStringFunction (parseLeadingNumber));
    }

    std::unique_ptr<juce::AudioParameterFloat> makePitchRangeParameter()
    {
        return std::make_unique<juce::AudioParameterFloat> (
            ParamIDs::pitchRange,
            "Pitch Range",
            juce::NormalisableRange<float> { ParamRanges::pitchRangeMinSemitones, ParamRanges::pitchRangeMaxSemitones, 1.0f },
            ParamRanges::pitchRangeDefaultSemitones,
            juce::AudioParameterFloatAttributes()
                .withLabel ("st")
                .withStringFromValueFunction (formatSemitones)
                .withValueFromStringFunction (parseLeadingNumber));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeDelayTimeParameter());
    layout.add (makePercentParameter (ParamIDs::mix, "Mix", 100.0f, ParamRanges::mixDefaultPercent));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::pingPong, "Ping-Pong", ParamRanges::pingPongDefault));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::sync, "Sync", ParamRanges::syncDefault));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParamIDs::division, "Division", TempoSync::getDivisionNames(), TempoSync::defaultDivisionIndex));
    layout.add (makeLengthParameter());
    layout.add (makeThresholdParameter());
    layout.add (makePercentParameter (ParamIDs::pingPongWidth, "Width", 100.0f, ParamRanges::pingPongWidthDefaultPercent));
    layout.add (makePitchRangeParameter());
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::pitchBipolar, "Bipolar", ParamRanges::pitchBipolarDefault));

    return layout;
}
