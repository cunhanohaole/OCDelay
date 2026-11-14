#include "Parameters.h"

namespace
{
    // One decimal for short times, so End Rate's 62.5 ms default (1/32 at 120 BPM) reads exactly
    juce::String formatMilliseconds (float value, int /*maximumStringLength*/)
    {
        const auto hasFraction = std::abs (value - std::round (value)) >= 0.05f;
        return value < 10.0f || (value < 100.0f && hasFraction) ? juce::String (value, 1) + " ms"
                                                                 : juce::String (juce::roundToInt (value)) + " ms";
    }

    juce::String formatPercent (float value, int /*maximumStringLength*/)
    {
        return juce::String (juce::roundToInt (value)) + " %";
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

    // Slice, Rate and End Rate: 1-2000 ms, skewed so short times get more of the knob
    std::unique_ptr<juce::AudioParameterFloat> makeMillisecondsParameter (const juce::ParameterID& id,
        const juce::String& name,
        float defaultMs = ParamRanges::delayTimeDefaultMs)
    {
        juce::NormalisableRange<float> range { ParamRanges::delayTimeMinMs, ParamRanges::delayTimeMaxMs };
        range.setSkewForCentre (ParamRanges::delayTimeSkewCentreMs);

        return std::make_unique<juce::AudioParameterFloat> (
            id,
            name,
            range,
            defaultMs,
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
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeMillisecondsParameter (ParamIDs::delayTime, "Slice"));
    layout.add (makePercentParameter (ParamIDs::mix, "Mix", 100.0f, ParamRanges::mixDefaultPercent));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::pingPong, "Ping-Pong", ParamRanges::pingPongDefault));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::sync, "Sync", ParamRanges::syncDefault));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParamIDs::division, "Division", TempoSync::getDivisionNames(), TempoSync::defaultDivisionIndex));
    layout.add (makeLengthParameter());
    layout.add (makeThresholdParameter());
    layout.add (makePercentParameter (ParamIDs::pingPongWidth, "Width", 100.0f, ParamRanges::pingPongWidthDefaultPercent));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::freeRate, "Free Rate", ParamRanges::freeRateDefault));
    layout.add (makeMillisecondsParameter (ParamIDs::rate, "Rate"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParamIDs::rateDivision, "Rate Division", TempoSync::getDivisionNames(), TempoSync::defaultDivisionIndex));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::rateOverlap, "Overlap", ParamRanges::rateOverlapDefault));
    layout.add (makeMillisecondsParameter (ParamIDs::endRate, "End Rate", ParamRanges::endRateDefaultMs));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParamIDs::endRateDivision, "End Rate Division", TempoSync::getDivisionNames(), ParamRanges::endRateDefaultDivisionIndex));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::maxSpeed, "Max Speed", ParamRanges::maxSpeedDefault));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::pitchFollow, "Pitch Follow", ParamRanges::pitchFollowDefault));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::stop, "Stop", false)); // momentary
    layout.add (makePercentParameter (ParamIDs::boost, "Boost", 100.0f, ParamRanges::boostDefaultPercent));

    return layout;
}
