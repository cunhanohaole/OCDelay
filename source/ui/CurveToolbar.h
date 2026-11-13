#pragma once

#include "../curve/CurveSlot.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Top bar of the curve editor: Volume / Speed tabs, the End Rate, Max Speed and Pitch Follow
// controls (shown on the Speed tab only) and the Snap toggle.
class CurveToolbar : public juce::Component
{
public:
    // Fraction of the width the tabs take at the left
    static constexpr float kTabsWidth = 0.22f;

    explicit CurveToolbar (juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

    CurveTarget getTarget() const { return target; }
    void setTarget (CurveTarget newTarget);
    bool isSnapOn() const { return snapButton.getToggleState(); }

    std::function<void()> onTargetChange;

private:
    void setUpTab (juce::TextButton& tab, CurveTarget tabTarget, int connectedEdges);
    void setUpSpeedControls();
    void updateSpeedControls();
    void layoutSpeedControls (juce::Rectangle<int> area);

    CurveTarget target = CurveTarget::volume;
    bool syncOn = false;
    bool maxSpeedOn = false;

    juce::TextButton volumeTab { "Volume" };
    juce::TextButton speedTab { "Speed" };
    juce::ToggleButton snapButton { "Snap" };

    // End Rate (ms) and End Rate Division share one slot; Sync decides which is shown
    juce::Label endRateLabel;
    juce::Slider endRateSlider { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::Slider endDivisionSlider { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::ToggleButton maxSpeedButton { "Max Speed" };
    juce::ToggleButton pitchFollowButton { "Pitch Follow" };
    juce::AudioProcessorValueTreeState::SliderAttachment endRateAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment endDivisionAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment maxSpeedAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment pitchFollowAttachment;

    // Follow Sync and Max Speed from any source (button, automation, preset) on the message thread
    juce::ParameterAttachment syncAttachment;
    juce::ParameterAttachment maxSpeedEnableAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveToolbar)
};
