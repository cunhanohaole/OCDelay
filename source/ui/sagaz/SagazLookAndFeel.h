#pragma once

#include "SagazFonts.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Draws JUCE's stock widgets in the Sagaz UI system (Graphite). Components pick a variant
// with the static setters, which tag the widget; everything else is styled automatically:
//   rotary sliders         270 deg knob with a value box below
//   LinearBar sliders      slider bar with the value printed inside
//   ToggleButtons          checkbox switch with its label
//   TextButtons            action button (default), tab or segment
class SagazLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum class ButtonStyle
    {
        action,
        tab,
        segment
    };

    SagazLookAndFeel();

    static void setButtonStyle (juce::Button& button, ButtonStyle style);
    static void setTextStyle (juce::Label& label, SagazTextStyle style);

    const SagazFonts& getFonts() const { return *fonts; }

    // Fonts
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;
    juce::Font getLabelFont (juce::Label& label) override;
    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override;

    // Labels
    void drawLabel (juce::Graphics& g, juce::Label& label) override;

    // Buttons
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour, bool isHighlighted, bool isDown) override;
    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool isHighlighted, bool isDown) override;
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isHighlighted, bool isDown) override;

    // Sliders
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position, float startAngle, float endAngle, juce::Slider& slider) override;
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float position, float minPosition, float maxPosition, juce::Slider::SliderStyle style, juce::Slider& slider) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider& slider) override;

private:
    void setUpColours();
    void drawSliderValue (juce::Graphics& g, juce::Label& label, juce::Slider& slider);
    void drawSliderBar (juce::Graphics& g, float position, juce::Slider& slider);
    void drawActionButton (juce::Graphics& g, juce::Button& button, bool isHighlighted, bool isDown);
    void drawTab (juce::Graphics& g, juce::Button& button, bool isHighlighted, bool isDown);
    void drawSegment (juce::Graphics& g, juce::Button& button, bool isHighlighted, bool isDown);
    void drawSwitchBox (juce::Graphics& g, juce::Rectangle<float> box, juce::ToggleButton& button, bool isHighlighted, bool isDown);

    juce::SharedResourcePointer<SagazFonts> fonts;
};
