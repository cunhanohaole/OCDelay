#pragma once

#include "../curve/CurveSlot.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Top bar of the curve editor: Volume / Pitch tabs, the pitch Range and Bipolar controls
// (shown on the Pitch tab only) and the Snap toggle.
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
    void setUpPitchControls();
    void layoutPitchControls (juce::Rectangle<int> area);

    CurveTarget target = CurveTarget::volume;

    juce::TextButton volumeTab { "Volume" };
    juce::TextButton pitchTab { "Pitch" };
    juce::ToggleButton snapButton { "Snap" };

    juce::Label rangeLabel;
    juce::Slider rangeSlider { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::ToggleButton bipolarButton { "Bipolar" };
    juce::AudioProcessorValueTreeState::SliderAttachment rangeAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bipolarAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveToolbar)
};
