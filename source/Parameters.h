#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs are part of saved DAW sessions and automation: never rename them once shipped.
namespace ParamIDs
{
    inline const juce::ParameterID delayTime { "delayTime", 1 };
    inline const juce::ParameterID feedback { "feedback", 1 };
    inline const juce::ParameterID mix { "mix", 1 };
    inline const juce::ParameterID pingPong { "pingPong", 1 };
}

namespace ParamRanges
{
    inline constexpr float delayTimeMinMs = 1.0f;
    inline constexpr float delayTimeMaxMs = 2000.0f;
    inline constexpr float delayTimeDefaultMs = 500.0f;
    inline constexpr float delayTimeSkewCentreMs = 250.0f;

    inline constexpr float feedbackMaxPercent = 95.0f;
    inline constexpr float feedbackDefaultPercent = 35.0f;

    inline constexpr float mixDefaultPercent = 50.0f;

    inline constexpr bool pingPongDefault = false;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
