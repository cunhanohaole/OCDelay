#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Segmented 1 / 2 / 4 / 8 / 16 / 32 bar buttons bound to the Length choice parameter.
// Follows automation and session loads through a ParameterAttachment.
class LengthSelector : public juce::Component
{
public:
    explicit LengthSelector (juce::RangedAudioParameter& lengthParameter);

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    int getSelectedIndex() const;

private:
    void showSelectedIndex (int index);

    juce::Label label;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::Rectangle<int> groupBounds;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LengthSelector)
};
