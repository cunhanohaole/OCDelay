#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Momentary Stop button: holds the Stop parameter on while pressed, as one host gesture,
// so the processor (and host automation) sees the press.
class StopButton : public juce::Component
{
public:
    explicit StopButton (juce::RangedAudioParameter& stopParameter);

    void resized() override;

    juce::TextButton& getButton() { return button; }

private:
    void followButtonState();
    void press();
    void release();

    juce::TextButton button { "Stop" };
    juce::ParameterAttachment attachment;
    bool held = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StopButton)
};
