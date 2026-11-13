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
    inline const juce::ParameterID pingPongWidth { "pingPongWidth", 3 };
    // Retired in v0.5 with the pitch curve (replaced by the speed curve). Never reuse: "pitchRange", "pitchBipolar"
    inline const juce::ParameterID freeRate { "freeRate", 4 };
    inline const juce::ParameterID rate { "rate", 4 };
    inline const juce::ParameterID rateDivision { "rateDivision", 4 };
    inline const juce::ParameterID rateOverlap { "rateOverlap", 4 };
    inline const juce::ParameterID endRate { "endRate", 5 };
    inline const juce::ParameterID endRateDivision { "endRateDivision", 5 };
    inline const juce::ParameterID maxSpeed { "maxSpeed", 5 };
    inline const juce::ParameterID pitchFollow { "pitchFollow", 5 };
    inline const juce::ParameterID stop { "stop", 6 };
}

namespace ParamRanges
{
    inline constexpr float delayTimeMinMs = 1.0f;
    inline constexpr float delayTimeMaxMs = 2000.0f;
    inline constexpr float delayTimeDefaultMs = 375.0f; // 1/8D at 120 BPM, the default Division
    inline constexpr float delayTimeSkewCentreMs = 250.0f;

    inline constexpr float mixDefaultPercent = 50.0f;

    inline constexpr bool pingPongDefault = true;
    inline constexpr bool syncDefault = true;

    // Changed in place before v0.2 was released. From now on only ever append choices.
    inline constexpr std::array<int, 6> lengthBarsOptions { 1, 2, 4, 8, 16, 32 };
    inline constexpr int lengthDefaultIndex = 2; // 4 bars

    inline constexpr float thresholdMinDb = -60.0f;
    inline constexpr float thresholdMaxDb = 0.0f;
    inline constexpr float thresholdDefaultDb = -30.0f;

    inline constexpr float pingPongWidthDefaultPercent = 100.0f;

    // Rate uses the same ms range and division list as Slice
    inline constexpr bool freeRateDefault = false;
    inline constexpr bool rateOverlapDefault = false;

    // End Rate uses the same ms range and division list as Slice; 1/32 = 62.5 ms at 120 BPM
    inline constexpr float endRateDefaultMs = 62.5f;
    inline constexpr int endRateDefaultDivisionIndex = 2; // 1/32
    inline constexpr bool maxSpeedDefault = true;
    inline constexpr bool pitchFollowDefault = false;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
