#pragma once

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

// The Sagaz lockup for plugin headers: the mark (two accent hooks) and the letters SAGAZ,
// both on an 18° shear. Vector paths from the Sagaz UI system, scaled to the component's height.
class SagazLogo : public juce::Component
{
public:
    SagazLogo();

    void paint (juce::Graphics&) override;

    // Width that keeps the lockup's proportions at the given height
    static int getWidthForHeight (int height);

private:
    juce::AffineTransform getDesignToComponentTransform() const;

    juce::Path mark;
    std::array<juce::Path, 5> letters; // S A G A Z, each with its own winding rule

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SagazLogo)
};
