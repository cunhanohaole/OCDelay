#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// A labelled rotary knob attached to one APVTS parameter. The value box shows the
// parameter's own text (with units) and accepts typed values.
class ParameterKnob : public juce::Component
{
public:
    ParameterKnob (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& parameterID, const juce::String& labelText);

    void resized() override;

    juce::Slider& getSlider() { return slider; }

private:
    juce::Label label;
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterKnob)
};
