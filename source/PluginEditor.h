#pragma once

#include "PluginProcessor.h"
#include "ui/CurveEditor.h"
#include "ui/LengthSelector.h"
#include "ui/OCDelayLookAndFeel.h"
#include "ui/ParameterKnob.h"

#if JUCE_DEBUG
    #include "melatonin_inspector/melatonin_inspector.h"
#endif

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    // Fixed size for v0.1. Layout is proportional, so resizing can be enabled later.
    static constexpr int kWidth = 640;
    static constexpr int kHeight = 400;

    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void layoutControlStrip (juce::Rectangle<int> strip);
    void layoutSwitches (juce::Rectangle<int> column);
    void showDivisionKnob (bool shouldShowDivision);

    // Declared first so it outlives every component that uses it
    OCDelayLookAndFeel lookAndFeel;

    CurveEditor curveEditor;

    // Slice (ms) and Division share one slot; Sync decides which is shown
    ParameterKnob delayTimeKnob;
    ParameterKnob divisionKnob;
    LengthSelector lengthSelector;
    ParameterKnob thresholdKnob;
    ParameterKnob mixKnob;

    juce::ToggleButton syncButton { "Sync" };
    juce::ToggleButton pingPongButton { "Ping-Pong" };
    juce::AudioProcessorValueTreeState::ButtonAttachment syncAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment pingPongAttachment;

    // Follows Sync from any source (button, automation, preset) on the message thread
    juce::ParameterAttachment syncVisibilityAttachment;

#if JUCE_DEBUG
    void setUpInspector();

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect" };
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
