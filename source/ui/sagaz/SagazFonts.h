#pragma once

#include <juce_graphics/juce_graphics.h>

// The text roles of the Sagaz UI system
enum class SagazTextStyle
{
    label, // control titles: 11 px semibold
    switchLabel, // switch and slider-bar labels: 11 px medium
    value, // value readouts: 11 px medium
    tab, // 12 px semibold
    button, // buttons and segments: 11 px semibold
    axis, // plot labels: 9 px semibold, upper case
    title // plugin name: 14 px bold
};

// The bundled Inter typefaces (SIL OFL, see licenses/Inter-OFL.txt), loaded once and shared
// through juce::SharedResourcePointer.
class SagazFonts
{
public:
    SagazFonts();

    juce::Font get (SagazTextStyle style) const;
    juce::Typeface::Ptr getMedium() const { return medium; }
    juce::Typeface::Ptr getBold() const { return bold; }

private:
    juce::Typeface::Ptr medium;
    juce::Typeface::Ptr semiBold;
    juce::Typeface::Ptr bold;
};
