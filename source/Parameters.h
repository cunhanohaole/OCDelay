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
    inline const juce::ParameterID pitchRange { "pitchRange", 3 };
    inline const juce::ParameterID pitchBipolar { "pitchBipolar", 3 };
    inline const juce::ParameterID freeRate { "freeRate", 4 };
    inline const juce::ParameterID rate { "rate", 4 };
    inline const juce::ParameterID rateDivision { "rateDivision", 4 };
    inline const juce::ParameterID rateOverlap { "rateOverlap", 4 };
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

    // Changed in place before v0.2 was released. From now on only ever append choices.
    inline constexpr std::array<int, 6> lengthBarsOptions { 1, 2, 4, 8, 16, 32 };
    inline constexpr int lengthDefaultIndex = 3; // 8 bars

    inline constexpr float thresholdMinDb = -60.0f;
    inline constexpr float thresholdMaxDb = 0.0f;
    inline constexpr float thresholdDefaultDb = -30.0f;

    inline constexpr float pingPongWidthDefaultPercent = 100.0f;

    inline constexpr float pitchRangeMinSemitones = 1.0f;
    inline constexpr float pitchRangeMaxSemitones = 48.0f;
    inline constexpr float pitchRangeDefaultSemitones = 12.0f;
    inline constexpr bool pitchBipolarDefault = false;

    // Rate uses the same ms range and division list as Slice
    inline constexpr bool freeRateDefault = false;
    inline constexpr bool rateOverlapDefault = false;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
