#pragma once

#include "TempoSync.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs are part of saved DAW sessions and automation: never rename them once shipped.
namespace ParamIDs
{
    inline const juce::ParameterID delayTime { "delayTime", 1 };
    // Retired in v0.2 (replaced by Length and the volume curve). Never reuse: "feedback"
    inline const juce::ParameterID mix { "mix", 1 };
    inline const juce::ParameterID pingPong { "pingPong", 1 };
    inline const juce::ParameterID sync { "sync", 1 };
    inline const juce::ParameterID division { "division", 1 };
    inline const juce::ParameterID length { "length", 2 };
    inline const juce::ParameterID threshold { "threshold", 2 };
}

namespace ParamRanges
{
    inline constexpr float delayTimeMinMs = 1.0f;
    inline constexpr float delayTimeMaxMs = 2000.0f;
    inline constexpr float delayTimeDefaultMs = 500.0f;
    inline constexpr float delayTimeSkewCentreMs = 250.0f;

    inline constexpr float mixDefaultPercent = 50.0f;

    inline constexpr bool pingPongDefault = false;
    inline constexpr bool syncDefault = false;

    inline constexpr std::array<int, 4> lengthBarsOptions { 4, 8, 16, 32 };
    inline constexpr int lengthDefaultIndex = 1; // 8 bars

    inline constexpr float thresholdMinDb = -60.0f;
    inline constexpr float thresholdMaxDb = 0.0f;
    inline constexpr float thresholdDefaultDb = -30.0f;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
