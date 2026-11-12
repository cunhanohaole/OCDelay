#include "LengthSelector.h"

namespace
{
    constexpr int kRadioGroupId = 1001;
}

LengthSelector::LengthSelector (juce::RangedAudioParameter& lengthParameter)
    : attachment (lengthParameter, [this] (float index) { showSelectedIndex (juce::roundToInt (index)); })
{
    label.setText ("Length", juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    const auto choices = lengthParameter.getAllValueStrings();

    for (int i = 0; i < choices.size(); ++i)
    {
        auto* button = buttons.add (std::make_unique<juce::TextButton> (choices[i]));
        button->setRadioGroupId (kRadioGroupId);
        button->setClickingTogglesState (true);
        button->setTitle ("Length " + choices[i] + " bars"); // accessibility name
        button->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };

        const auto connectLeft = i > 0 ? juce::Button::ConnectedOnLeft : 0;
        const auto connectRight = i < choices.size() - 1 ? juce::Button::ConnectedOnRight : 0;
        button->setConnectedEdges (connectLeft | connectRight);

        addAndMakeVisible (button);
    }

    attachment.sendInitialUpdate();
}

void LengthSelector::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (proportionOfHeight (0.18f)));

    // One row of buttons, vertically centred where a knob would sit
    auto row = area.withSizeKeepingCentre (area.getWidth(), proportionOfHeight (0.24f));
    const auto buttonWidth = row.getWidth() / juce::jmax (1, buttons.size());

    for (auto* button : buttons)
        button->setBounds (row.removeFromLeft (buttonWidth));
}

int LengthSelector::getSelectedIndex() const
{
    for (int i = 0; i < buttons.size(); ++i)
        if (buttons[i]->getToggleState())
            return i;

    return -1;
}

void LengthSelector::showSelectedIndex (int index)
{
    if (juce::isPositiveAndBelow (index, buttons.size()))
        buttons[index]->setToggleState (true, juce::dontSendNotification);
}
