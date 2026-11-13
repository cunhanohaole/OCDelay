#include "ParameterKnob.h"

ParameterKnob::ParameterKnob (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& parameterID, const juce::String& labelText)
    : attachment (apvts, parameterID.getParamID(), slider)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    slider.setTextBoxIsEditable (true);
    slider.setTitle (labelText); // accessibility name
    addAndMakeVisible (slider);
}

void ParameterKnob::enablementChanged()
{
    setAlpha (isEnabled() ? 1.0f : 0.35f);
}

void ParameterKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (proportionOfHeight (0.18f)));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, proportionOfWidth (0.6f), proportionOfHeight (0.18f));
    slider.setBounds (area);
}
