#pragma once

#include <juce_graphics/juce_graphics.h>

// Design tokens of the Sagaz plugin UI system, "Graphite" direction: cool near-black levels,
// flat fills and 1 px hairlines. Shared by every Sagaz plugin, so nothing here knows OCDelay.
namespace SagazTheme
{
    namespace Colours
    {
        // Backgrounds
        inline const juce::Colour window { 0xff111316 };
        inline const juce::Colour panel { 0xff171a1e };
        inline const juce::Colour inset { 0xff0d0f12 }; // plots, value boxes, tracks

        // Surfaces and lines
        inline const juce::Colour surface { 0xff21252b };
        inline const juce::Colour surfaceHover { 0xff2a2f36 };
        inline const juce::Colour surfacePressed { 0xff1a1d22 };
        inline const juce::Colour border { 0xff2b3037 };
        inline const juce::Colour borderStrong { 0xff3b414a };

        // Text
        inline const juce::Colour textPrimary { 0xffe6e9ec };
        inline const juce::Colour textSecondary { 0xff949ba4 };
        inline const juce::Colour textDisabled { 0xff4d535b };
        inline const juce::Colour textOnAccent { 0xff08131a };

        // Accents
        inline const juce::Colour accent { 0xff4cb8e0 };
        inline const juce::Colour accentHover { 0xff74cbeb };
        inline const juce::Colour accentPressed { 0xff2e9ac2 };
        inline const juce::Colour accentSoft { 0x244cb8e0 }; // area under the primary curve
        inline const juce::Colour accentFill { 0x4d4cb8e0 }; // slider-bar fill
        inline const juce::Colour accent2 { 0xffe6b450 }; // secondary channel, e.g. a speed curve
        inline const juce::Colour accent2Soft { 0x21e6b450 };

        // Plot
        inline const juce::Colour gridSub { 0xff14171b };
        inline const juce::Colour gridBeat { 0xff1e2329 };
        inline const juce::Colour gridBar { 0xff2e343c };
        inline const juce::Colour playhead { 0xfff4f6f8 };
        inline const juce::Colour marker { 0xff4cb8e0 };

        // Knob
        inline const juce::Colour knobTrack { 0xff262b31 };
        inline const juce::Colour knobBody { 0xff1b1f24 };
        inline const juce::Colour knobBodyHover { 0xff22272d };
        inline const juce::Colour knobBodyPressed { 0xff16191d };
        inline const juce::Colour knobStroke { 0xff30353c };
    }

    namespace Radius
    {
        inline constexpr float small = 3.0f; // boxes, switches, buttons
        inline constexpr float medium = 4.0f; // segments, tabs, plot
        inline constexpr float large = 6.0f; // panels
    }

    namespace Stroke
    {
        inline constexpr float hair = 1.0f;
        inline constexpr float control = 1.0f;
        inline constexpr float arc = 3.0f;
        inline constexpr float curve = 1.5f;
        inline constexpr float pointer = 2.0f;
        inline constexpr float playhead = 1.5f;
    }

    // Control sizes in px, for any plugin
    namespace Metrics
    {
        inline constexpr int knob = 48;
        inline constexpr int knobCellWidth = 80;
        inline constexpr int stackGap = 6; // title -> knob -> value box
        inline constexpr int valueBoxHeight = 18;
        inline constexpr int valueBoxWidth = 54;
        inline constexpr int switchBox = 14;
        inline constexpr int switchLabelGap = 7;
        inline constexpr int buttonHeight = 20; // also slider bars
        inline constexpr int buttonMinWidth = 56;
        inline constexpr int sliderBarWidth = 72;
        inline constexpr int segmentHeight = 22;
        inline constexpr int segmentWidth = 26;
        inline constexpr int tabHeight = 24;
        inline constexpr int tabWidth = 66;
        inline constexpr int controlGap = 14; // between controls in a row
        inline constexpr int labelHeight = 14;
    }
}
