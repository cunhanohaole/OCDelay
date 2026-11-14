#pragma once

#include "../curve/CurveSlot.h"
#include "sagaz/SagazFonts.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Top bar of the curve editor, left to right: Volume / Speed tabs, then on the Speed tab only
// Boost, End Rate, Max Speed and Pitch Follow; Snap at the far right.
class CurveToolbar : public juce::Component
{
public:
    explicit CurveToolbar (juce::AudioProcessorValueTreeState& apvts);

    void paint (juce::Graphics&) override;
    void resized() override;

    // The empty stretch between the speed controls and Snap (the same on both tabs)
    juce::Rectangle<int> getFreeArea() const { return freeArea; }

    CurveTarget getTarget() const { return target; }
    void setTarget (CurveTarget newTarget);
    bool isSnapOn() const { return snapButton.getToggleState(); }

    std::function<void()> onTargetChange;

private:
    void setUpTab (juce::TextButton& tab, CurveTarget tabTarget, int connectedEdges);
    void setUpSpeedControls();
    void updateSpeedControls();
    int layoutTabs (int left);
    int layoutSpeedControls (int left);
    int layoutSliderBar (juce::Label& label, std::initializer_list<juce::Slider*> sliders, int left);
    int layoutSwitch (juce::ToggleButton& button, int left);
    int textWidth (const juce::String& text, SagazTextStyle style) const;

    CurveTarget target = CurveTarget::volume;
    bool syncOn = false;
    bool maxSpeedOn = false;
    juce::Rectangle<int> tabsFrame;
    int separatorX = 0;
    juce::Rectangle<int> freeArea;
    juce::SharedResourcePointer<SagazFonts> fonts;

    juce::TextButton volumeTab { "Volume" };
    juce::TextButton speedTab { "Speed" };
    juce::ToggleButton snapButton { "Snap" };

    // End Rate (ms) and End Rate Division share one slot; Sync decides which is shown
    juce::Label endRateLabel;
    juce::Slider endRateSlider { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::Slider endDivisionSlider { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::ToggleButton maxSpeedButton { "Max Speed" };
    juce::ToggleButton pitchFollowButton { "Pitch Follow" };
    juce::Label boostLabel;
    juce::Slider boostSlider { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::AudioProcessorValueTreeState::SliderAttachment endRateAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment endDivisionAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment maxSpeedAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment pitchFollowAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment boostAttachment;

    // Follow Sync and Max Speed from any source (button, automation, preset) on the message thread
    juce::ParameterAttachment syncAttachment;
    juce::ParameterAttachment maxSpeedEnableAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveToolbar)
};
