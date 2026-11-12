#include "CurveToolbar.h"
#include "../Parameters.h"

namespace
{
    constexpr int kTabRadioGroupId = 2001;
}

CurveToolbar::CurveToolbar (juce::AudioProcessorValueTreeState& apvts)
    : rangeAttachment (apvts, ParamIDs::pitchRange.getParamID(), rangeSlider),
      bipolarAttachment (apvts, ParamIDs::pitchBipolar.getParamID(), bipolarButton)
{
    setUpTab (volumeTab, CurveTarget::volume, juce::Button::ConnectedOnRight);
    setUpTab (pitchTab, CurveTarget::pitch, juce::Button::ConnectedOnLeft);
    volumeTab.setToggleState (true, juce::dontSendNotification);

    snapButton.setToggleState (true, juce::dontSendNotification);
    snapButton.setTooltip ("Snap points to the beat grid (and to whole semitones on the Pitch tab)");
    addAndMakeVisible (snapButton);

    setUpPitchControls();
}

void CurveToolbar::setUpTab (juce::TextButton& tab, CurveTarget tabTarget, int connectedEdges)
{
    tab.setRadioGroupId (kTabRadioGroupId);
    tab.setClickingTogglesState (true);
    tab.setConnectedEdges (connectedEdges);
    tab.onClick = [this, tabTarget] { setTarget (tabTarget); };
    addAndMakeVisible (tab);
}

void CurveToolbar::setUpPitchControls()
{
    rangeLabel.setText ("Range", juce::dontSendNotification);
    rangeLabel.setJustificationType (juce::Justification::centredRight);
    rangeSlider.setTitle ("Pitch range"); // accessibility name
    bipolarButton.setTooltip ("Off: the curve goes from no change up to +Range. On: the middle is no change");

    addChildComponent (rangeLabel);
    addChildComponent (rangeSlider);
    addChildComponent (bipolarButton);
}

void CurveToolbar::setTarget (CurveTarget newTarget)
{
    (newTarget == CurveTarget::pitch ? pitchTab : volumeTab).setToggleState (true, juce::dontSendNotification);

    const auto showPitch = newTarget == CurveTarget::pitch;
    rangeLabel.setVisible (showPitch);
    rangeSlider.setVisible (showPitch);
    bipolarButton.setVisible (showPitch);

    if (newTarget == target)
        return;

    target = newTarget;
    if (onTargetChange)
        onTargetChange();
}

void CurveToolbar::resized()
{
    auto area = getLocalBounds();
    auto tabs = area.removeFromLeft (proportionOfWidth (kTabsWidth)).reduced (2);
    volumeTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2));
    pitchTab.setBounds (tabs);

    snapButton.setBounds (area.removeFromRight (proportionOfWidth (0.11f)));
    layoutPitchControls (area.removeFromRight (proportionOfWidth (0.38f)));
}

// Range label, range bar and Bipolar, right-aligned next to Snap
void CurveToolbar::layoutPitchControls (juce::Rectangle<int> area)
{
    bipolarButton.setBounds (area.removeFromRight (proportionOfWidth (0.13f)));
    rangeSlider.setBounds (area.removeFromRight (proportionOfWidth (0.15f)).reduced (2, 4));
    rangeLabel.setBounds (area);
}
