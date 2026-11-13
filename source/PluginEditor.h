#pragma once

#include "PluginProcessor.h"
#include "ui/CurveEditor.h"
#include "ui/LengthSelector.h"
#include "ui/OCDelayLookAndFeel.h"
#include "ui/ParameterKnob.h"
#include "ui/StopButton.h"

#if JUCE_DEBUG
    #include "melatonin_inspector/melatonin_inspector.h"
#endif

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    // Fixed size for now. Layout is proportional, so resizing can be enabled later.
    static constexpr int kWidth = 860;
    static constexpr int kHeight = 460;

    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void layoutControlStrip (juce::Rectangle<int> strip);
    void layoutLengthColumn (juce::Rectangle<int> column);
    void layoutSwitches (juce::Rectangle<int> column);
    void showDivisionKnobs (bool shouldShowDivisions);
    void enableRateControls (bool freeRateOn);

    // Declared first so it outlives every component that uses it
    OCDelayLookAndFeel lookAndFeel;

    CurveEditor curveEditor;

    // Slice (ms) and Division share one slot; Sync decides which is shown
    ParameterKnob delayTimeKnob;
    ParameterKnob divisionKnob;

    // Rate (ms) and Rate Division share the next slot; enabled only with Free Rate on
    ParameterKnob rateKnob;
    ParameterKnob rateDivisionKnob;
    LengthSelector lengthSelector;
    StopButton stopButton;
    ParameterKnob thresholdKnob;
    ParameterKnob mixKnob;
    ParameterKnob widthKnob;

    juce::ToggleButton syncButton { "Sync" };
    juce::ToggleButton freeRateButton { "Free Rate" };
    juce::ToggleButton overlapButton { "Overlap" };
    juce::ToggleButton pingPongButton { "Ping-Pong" };
    juce::AudioProcessorValueTreeState::ButtonAttachment syncAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment freeRateAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment overlapAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment pingPongAttachment;

    // Follow Sync and Free Rate from any source (button, automation, preset) on the message thread
    juce::ParameterAttachment syncVisibilityAttachment;
    juce::ParameterAttachment freeRateEnableAttachment;

#if JUCE_DEBUG
    void setUpInspector();

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect" };
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
