#pragma once

#include "PluginProcessor.h"
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

    // Declared first so it outlives every component that uses it
    OCDelayLookAndFeel lookAndFeel;

    ParameterKnob delayTimeKnob;
    ParameterKnob feedbackKnob;
    ParameterKnob mixKnob;
    juce::ToggleButton pingPongButton { "Ping-Pong" };
    juce::AudioProcessorValueTreeState::ButtonAttachment pingPongAttachment;

#if JUCE_DEBUG
    void setUpInspector();

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect" };
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
