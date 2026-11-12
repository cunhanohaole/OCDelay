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
            "Delay Time",
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

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeDelayTimeParameter());
    layout.add (makePercentParameter (ParamIDs::feedback, "Feedback", ParamRanges::feedbackMaxPercent, ParamRanges::feedbackDefaultPercent));
    layout.add (makePercentParameter (ParamIDs::mix, "Mix", 100.0f, ParamRanges::mixDefaultPercent));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::pingPong, "Ping-Pong", ParamRanges::pingPongDefault));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::sync, "Sync", ParamRanges::syncDefault));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParamIDs::division, "Division", TempoSync::getDivisionNames(), TempoSync::defaultDivisionIndex));

    return layout;
}
