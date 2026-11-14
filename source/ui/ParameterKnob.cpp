#include "ParameterKnob.h"
#include "sagaz/SagazLookAndFeel.h"
#include "sagaz/SagazTheme.h"

ParameterKnob::ParameterKnob (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& parameterID, const juce::String& labelText)
    : attachment (apvts, parameterID.getParamID(), slider)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    SagazLookAndFeel::setTextStyle (label, SagazTextStyle::label);
    addAndMakeVisible (label);

    // 270 degrees, from 7:30 to 4:30
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxIsEditable (true);
    slider.setTitle (labelText); // accessibility name
    addAndMakeVisible (slider);
}

// Title, then the knob and its value box (laid out by the look-and-feel)
void ParameterKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (SagazTheme::Metrics::labelHeight));
    area.removeFromTop (SagazTheme::Metrics::stackGap);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, SagazTheme::Metrics::valueBoxWidth, SagazTheme::Metrics::valueBoxHeight);
    slider.setBounds (area);
}
