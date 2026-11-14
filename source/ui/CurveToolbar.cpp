#include "CurveToolbar.h"
#include "../Parameters.h"
#include "sagaz/SagazLookAndFeel.h"
#include "sagaz/SagazTheme.h"

namespace
{
    constexpr int kTabRadioGroupId = 2001;
    constexpr int kSeparatorHeight = 16;

    namespace Metrics = SagazTheme::Metrics;

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
    SagazLookAndFeel::setButtonStyle (tab, SagazLookAndFeel::ButtonStyle::tab);
    addAndMakeVisible (tab);
}

void CurveToolbar::setUpSpeedControls()
{
    endRateLabel.setText ("End", juce::dontSendNotification);
    endRateSlider.setTitle ("End Rate"); // accessibility names
    endDivisionSlider.setTitle ("End Rate");
    maxSpeedButton.setTooltip ("The top of the speed curve is the fastest rate: repeats blur into a buzz");
    pitchFollowButton.setTooltip ("Repeats play faster as they speed up, like tape, so the pitch rises with the speed");
    boostLabel.setText ("Boost", juce::dontSendNotification);

    for (auto* label : { &boostLabel, &endRateLabel })
    {
        label->setJustificationType (juce::Justification::centredLeft);
        label->setColour (juce::Label::textColourId, SagazTheme::Colours::textSecondary);
        label->setBorderSize ({});
        SagazLookAndFeel::setTextStyle (*label, SagazTextStyle::switchLabel);
    }
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
    repaint();

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

// The tab strip's frame, and the separator before the speed controls
void CurveToolbar::paint (juce::Graphics& g)
{
    const auto frame = tabsFrame.toFloat().reduced (0.5f);
    g.setColour (SagazTheme::Colours::inset);
    g.fillRoundedRectangle (frame, SagazTheme::Radius::medium);
    g.setColour (SagazTheme::Colours::border);
    g.drawRoundedRectangle (frame, SagazTheme::Radius::medium, SagazTheme::Stroke::control);

    if (target == CurveTarget::speed)
        g.fillRect (separatorX, (getHeight() - kSeparatorHeight) / 2, 1, kSeparatorHeight);
}

// Fixed sizes from the UI system; the speed controls keep their places on both tabs
void CurveToolbar::resized()
{
    const auto speedLeft = layoutTabs (0) + Metrics::controlGap;
    separatorX = speedLeft;
    const auto speedRight = layoutSpeedControls (separatorX + 1 + Metrics::controlGap);

    const auto snapWidth = Metrics::switchBox + Metrics::switchLabelGap + textWidth (snapButton.getButtonText(), SagazTextStyle::switchLabel);
    const auto snapLeft = getWidth() - snapWidth;
    layoutSwitch (snapButton, snapLeft);

    freeArea = { speedRight + Metrics::controlGap, 0, juce::jmax (0, snapLeft - Metrics::controlGap - speedRight - Metrics::controlGap), getHeight() };
}

// Two tabs inside an inset frame with a 2 px padding; returns the right edge
int CurveToolbar::layoutTabs (int left)
{
    constexpr int padding = 2;
    const auto cellHeight = Metrics::tabHeight - 2 * (padding + 1);
    tabsFrame = { left, (getHeight() - Metrics::tabHeight) / 2, 2 * Metrics::tabWidth + 3 * padding + 2, Metrics::tabHeight };

    auto cells = tabsFrame.reduced (padding + 1);
    volumeTab.setBounds (cells.removeFromLeft (Metrics::tabWidth).withHeight (cellHeight));
    cells.removeFromLeft (padding);
    speedTab.setBounds (cells.removeFromLeft (Metrics::tabWidth).withHeight (cellHeight));
    return tabsFrame.getRight();
}

// Boost, End Rate, Max Speed, Pitch Follow; returns the right edge
int CurveToolbar::layoutSpeedControls (int left)
{
    left = layoutSliderBar (boostLabel, { &boostSlider }, left) + Metrics::controlGap;
    left = layoutSliderBar (endRateLabel, { &endRateSlider, &endDivisionSlider }, left) + Metrics::controlGap;
    left = layoutSwitch (maxSpeedButton, left) + Metrics::controlGap;
    return layoutSwitch (pitchFollowButton, left);
}

// A label, then the bar a stack gap after it (End Rate's two bars share one place)
int CurveToolbar::layoutSliderBar (juce::Label& label, std::initializer_list<juce::Slider*> sliders, int left)
{
    const auto labelWidth = textWidth (label.getText(), SagazTextStyle::switchLabel);
    label.setBounds (left, 0, labelWidth, getHeight());

    const auto bar = juce::Rectangle<int> (left + labelWidth + Metrics::stackGap, (getHeight() - Metrics::buttonHeight) / 2, Metrics::sliderBarWidth, Metrics::buttonHeight);
    for (auto* slider : sliders)
        slider->setBounds (bar);

    return bar.getRight();
}

int CurveToolbar::layoutSwitch (juce::ToggleButton& button, int left)
{
    const auto width = Metrics::switchBox + Metrics::switchLabelGap + textWidth (button.getButtonText(), SagazTextStyle::switchLabel);
    button.setBounds (left, 0, width, getHeight());
    return left + width;
}

int CurveToolbar::textWidth (const juce::String& text, SagazTextStyle style) const
{
    return juce::GlyphArrangement::getStringWidthInt (fonts->get (style), text) + 1;
}
