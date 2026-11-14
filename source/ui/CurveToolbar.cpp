#include "CurveToolbar.h"
#include "../Parameters.h"

namespace
{
    constexpr int kTabRadioGroupId = 2001;

    juce::RangedAudioParameter& getParameter (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id)
    {
        auto* parameter = apvts.getParameter (id.getParamID());
        jassert (parameter != nullptr);
        return *parameter;
    }
}

CurveToolbar::CurveToolbar (juce::AudioProcessorValueTreeState& apvts)
    : endRateAttachment (apvts, ParamIDs::endRate.getParamID(), endRateSlider),
      endDivisionAttachment (apvts, ParamIDs::endRateDivision.getParamID(), endDivisionSlider),
      maxSpeedAttachment (apvts, ParamIDs::maxSpeed.getParamID(), maxSpeedButton),
      pitchFollowAttachment (apvts, ParamIDs::pitchFollow.getParamID(), pitchFollowButton),
      boostAttachment (apvts, ParamIDs::boost.getParamID(), boostSlider),
      syncAttachment (getParameter (apvts, ParamIDs::sync), [this] (float value) {
          syncOn = value >= 0.5f;
          updateSpeedControls();
      }),
      maxSpeedEnableAttachment (getParameter (apvts, ParamIDs::maxSpeed), [this] (float value) {
          maxSpeedOn = value >= 0.5f;
          updateSpeedControls();
      })
{
    setUpTab (volumeTab, CurveTarget::volume, juce::Button::ConnectedOnRight);
    setUpTab (speedTab, CurveTarget::speed, juce::Button::ConnectedOnLeft);
    volumeTab.setToggleState (true, juce::dontSendNotification);

    snapButton.setToggleState (true, juce::dontSendNotification);
    snapButton.setTooltip ("Snap points to the beat grid");
    addAndMakeVisible (snapButton);

    setUpSpeedControls();
    syncAttachment.sendInitialUpdate();
    maxSpeedEnableAttachment.sendInitialUpdate();
}

void CurveToolbar::setUpTab (juce::TextButton& tab, CurveTarget tabTarget, int connectedEdges)
{
    tab.setRadioGroupId (kTabRadioGroupId);
    tab.setClickingTogglesState (true);
    tab.setConnectedEdges (connectedEdges);
    tab.onClick = [this, tabTarget] { setTarget (tabTarget); };
    addAndMakeVisible (tab);
}

void CurveToolbar::setUpSpeedControls()
{
    endRateLabel.setText ("End", juce::dontSendNotification);
    endRateLabel.setJustificationType (juce::Justification::centredRight);
    endRateSlider.setTitle ("End Rate"); // accessibility names
    endDivisionSlider.setTitle ("End Rate");
    maxSpeedButton.setTooltip ("The top of the speed curve is the fastest rate: repeats blur into a buzz");
    pitchFollowButton.setTooltip ("Repeats play faster as they speed up, like tape, so the pitch rises with the speed");
    boostLabel.setText ("Boost", juce::dontSendNotification);
    boostLabel.setJustificationType (juce::Justification::centredRight);
    boostSlider.setTitle ("Boost");
    boostSlider.setTooltip ("Makes the fast part louder, rising with the speed curve: 100% = twice as loud at the top");

    for (auto* component : std::initializer_list<juce::Component*> { &boostLabel, &boostSlider, &endRateLabel, &endRateSlider, &endDivisionSlider, &maxSpeedButton, &pitchFollowButton })
        addChildComponent (component);
}

void CurveToolbar::setTarget (CurveTarget newTarget)
{
    (newTarget == CurveTarget::speed ? speedTab : volumeTab).setToggleState (true, juce::dontSendNotification);

    const auto changed = newTarget != target;
    target = newTarget;
    updateSpeedControls();

    if (changed && onTargetChange)
        onTargetChange();
}

// Visible on the Speed tab only; End Rate follows Sync and is greyed out while Max Speed is on
void CurveToolbar::updateSpeedControls()
{
    const auto showSpeed = target == CurveTarget::speed;
    endRateLabel.setVisible (showSpeed);
    maxSpeedButton.setVisible (showSpeed);
    pitchFollowButton.setVisible (showSpeed);
    boostLabel.setVisible (showSpeed);
    boostSlider.setVisible (showSpeed);
    endRateSlider.setVisible (showSpeed && ! syncOn);
    endDivisionSlider.setVisible (showSpeed && syncOn);

    endRateLabel.setEnabled (! maxSpeedOn);
    endRateSlider.setEnabled (! maxSpeedOn);
    endDivisionSlider.setEnabled (! maxSpeedOn);
}

void CurveToolbar::resized()
{
    auto area = getLocalBounds();
    auto tabs = area.removeFromLeft (proportionOfWidth (kTabsWidth)).reduced (2);
    volumeTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2));
    speedTab.setBounds (tabs);

    // Leaves room after the tabs for the debug build's Inspect button
    snapButton.setBounds (area.removeFromRight (proportionOfWidth (0.1f)));
    layoutSpeedControls (area.removeFromRight (proportionOfWidth (0.565f)));
}

// "Boost" and its bar, "End" and the End Rate bar, Max Speed and Pitch Follow, right-aligned next to Snap
void CurveToolbar::layoutSpeedControls (juce::Rectangle<int> area)
{
    pitchFollowButton.setBounds (area.removeFromRight (proportionOfWidth (0.14f)));
    maxSpeedButton.setBounds (area.removeFromRight (proportionOfWidth (0.13f)));

    const auto sliderArea = area.removeFromRight (proportionOfWidth (0.12f)).reduced (2, 4);
    endRateSlider.setBounds (sliderArea);
    endDivisionSlider.setBounds (sliderArea);
    endRateLabel.setBounds (area.removeFromRight (proportionOfWidth (0.04f)));

    boostSlider.setBounds (area.removeFromRight (proportionOfWidth (0.08f)).reduced (2, 4));
    boostLabel.setBounds (area);
}
