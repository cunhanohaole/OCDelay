#include "LengthSelector.h"
#include "sagaz/SagazLookAndFeel.h"
#include "sagaz/SagazTheme.h"

namespace
{
    constexpr int kRadioGroupId = 1001;
    constexpr int kRowTop = 32; // the row sits level with the knobs' centres
}

LengthSelector::LengthSelector (juce::RangedAudioParameter& lengthParameter)
    : attachment (lengthParameter, [this] (float index) { showSelectedIndex (juce::roundToInt (index)); })
{
    label.setText ("Length", juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    SagazLookAndFeel::setTextStyle (label, SagazTextStyle::label);
    addAndMakeVisible (label);

    const auto choices = lengthParameter.getAllValueStrings();

    for (int i = 0; i < choices.size(); ++i)
    {
        auto* button = buttons.add (std::make_unique<juce::TextButton> (choices[i]));
        button->setRadioGroupId (kRadioGroupId);
        button->setClickingTogglesState (true);
        button->setTitle ("Length " + choices[i] + " bars"); // accessibility name
        button->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        SagazLookAndFeel::setButtonStyle (*button, SagazLookAndFeel::ButtonStyle::segment);

        const auto connectLeft = i > 0 ? juce::Button::ConnectedOnLeft : 0;
        const auto connectRight = i < choices.size() - 1 ? juce::Button::ConnectedOnRight : 0;
        button->setConnectedEdges (connectLeft | connectRight);

        addAndMakeVisible (button);
    }

    attachment.sendInitialUpdate();
}

void LengthSelector::resized()
{
    label.setBounds (getLocalBounds().removeFromTop (SagazTheme::Metrics::labelHeight));

    const auto rowWidth = SagazTheme::Metrics::segmentWidth * buttons.size();
    auto row = juce::Rectangle<int> (rowWidth, SagazTheme::Metrics::segmentHeight).withCentre ({ getWidth() / 2, kRowTop + SagazTheme::Metrics::segmentHeight / 2 });
    groupBounds = row.expanded (1);

    for (auto* button : buttons)
        button->setBounds (row.removeFromLeft (SagazTheme::Metrics::segmentWidth));
}

// The group's outline, over the cells' fills
void LengthSelector::paintOverChildren (juce::Graphics& g)
{
    g.setColour (SagazTheme::Colours::borderStrong);
    g.drawRoundedRectangle (groupBounds.toFloat().reduced (0.5f), SagazTheme::Radius::medium, SagazTheme::Stroke::control);
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
