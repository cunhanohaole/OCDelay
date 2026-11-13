#include "StopButton.h"

StopButton::StopButton (juce::RangedAudioParameter& stopParameter)
    : attachment (stopParameter, [] (float) {}) // nothing to show: the button's own down state is the feedback
{
    button.setTooltip ("Stop the repeats (they fade out over 3 ms)");
    button.onStateChange = [this] { followButtonState(); };
    addAndMakeVisible (button);
}

void StopButton::resized()
{
    button.setBounds (getLocalBounds());
}

void StopButton::followButtonState()
{
    if (button.isDown() && ! held)
        press();
    else if (! button.isDown() && held)
        release();
}

void StopButton::press()
{
    held = true;
    attachment.beginGesture();
    attachment.setValueAsPartOfGesture (1.0f);
}

void StopButton::release()
{
    held = false;
    attachment.setValueAsPartOfGesture (0.0f);
    attachment.endGesture();
}
