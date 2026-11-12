#include "helpers/test_helpers.h"
#include <Parameters.h>
#include <PluginEditor.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
    // Strip controls, excluding the curve editor and the debug-only inspector button
    juce::Array<juce::Component*> getControls (juce::Component& editor)
    {
        juce::Array<juce::Component*> controls;

        for (auto* child : editor.getChildren())
            if (dynamic_cast<juce::TextButton*> (child) == nullptr && dynamic_cast<CurveEditor*> (child) == nullptr)
                controls.add (child);

        return controls;
    }

    template <typename ComponentType>
    ComponentType* findChild (juce::Component& editor)
    {
        for (auto* child : editor.getChildren())
            if (auto* match = dynamic_cast<ComponentType*> (child))
                return match;

        return nullptr;
    }

    int countVisible (const juce::Array<juce::Component*>& components)
    {
        int count = 0;
        for (auto* component : components)
            count += component->isVisible() ? 1 : 0;
        return count;
    }
}

TEST_CASE ("Editor has the fixed v0.1 size", "[editor]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = plugin.getActiveEditor();
        REQUIRE (editor != nullptr);
        CHECK (editor->getWidth() == PluginEditor::kWidth);
        CHECK (editor->getHeight() == PluginEditor::kHeight);
        CHECK_FALSE (editor->isResizable());
    });
}

TEST_CASE ("Editor controls sit in the bottom strip, inside the window", "[editor]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = plugin.getActiveEditor();
        const auto bounds = editor->getLocalBounds();
        const auto stripTop = bounds.getHeight() * 7 / 10;
        const auto controls = getControls (*editor);

        // Slice, Division (hidden), Length, Threshold, Mix, Width, Sync, Ping-Pong
        CHECK (controls.size() == 8);
        CHECK (countVisible (controls) == 7);

        for (auto* control : controls)
        {
            CHECK (bounds.contains (control->getBounds()));
            CHECK (control->getY() >= stripTop);
        }
    });
}

TEST_CASE ("Sync swaps the Slice knob for the Division knob", "[editor]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = plugin.getActiveEditor();
        auto* syncParameter = plugin.getAPVTS().getParameter (ParamIDs::sync.getParamID());

        auto isSliderShownFor = [editor] (const juce::String& title) {
            for (auto* control : getControls (*editor))
                if (auto* knob = dynamic_cast<ParameterKnob*> (control))
                    if (knob->getSlider().getTitle() == title && knob->isVisible())
                        return knob->getSlider().getTextFromValue (knob->getSlider().getValue());
            return juce::String();
        };

        CHECK (isSliderShownFor ("Slice").endsWith ("ms"));

        syncParameter->setValueNotifyingHost (1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        CHECK (isSliderShownFor ("Slice") == "1/4");

        syncParameter->setValueNotifyingHost (0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        CHECK (isSliderShownFor ("Slice").endsWith ("ms"));
    });
}

TEST_CASE ("Curve editor fills the area above the strip", "[editor]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = plugin.getActiveEditor();
        auto* curveEditor = findChild<CurveEditor> (*editor);
        REQUIRE (curveEditor != nullptr);

        const auto stripTop = editor->getHeight() * 7 / 10;
        CHECK (curveEditor->getBottom() <= stripTop);
        CHECK (curveEditor->getHeight() > editor->getHeight() / 2);
        CHECK (curveEditor->getWidth() > editor->getWidth() * 9 / 10);
    });
}

TEST_CASE ("Curve editor follows the processor's curve", "[editor]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* curveEditor = findChild<CurveEditor> (*plugin.getActiveEditor());
        REQUIRE (curveEditor != nullptr);
        CHECK (curveEditor->getCurve() == EnvelopeCurve {});

        // e.g. a session load replacing the curve while the editor is open
        auto replaced = plugin.getCurve();
        replaced.addPoint (0.5f, 1.0f);
        plugin.updateCurve (replaced);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        CHECK (curveEditor->getCurve() == replaced);
    });
}

TEST_CASE ("Length buttons follow the parameter", "[editor]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* selector = findChild<LengthSelector> (*plugin.getActiveEditor());
        REQUIRE (selector != nullptr);
        CHECK (selector->getSelectedIndex() == 3); // 8 bars

        auto* length = plugin.getAPVTS().getParameter (ParamIDs::length.getParamID());
        length->setValueNotifyingHost (length->convertTo0to1 (5.0f)); // 32 bars
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        CHECK (selector->getSelectedIndex() == 5);
        CHECK (plugin.getLengthBars() == 32);
    });
}

TEST_CASE ("Pitch tab shows and edits the pitch curve", "[editor]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* curveEditor = findChild<CurveEditor> (*plugin.getActiveEditor());
        REQUIRE (curveEditor != nullptr);

        curveEditor->getToolbar().setTarget (CurveTarget::pitch);
        CHECK (curveEditor->getCurve() == plugin.getCurve (CurveTarget::pitch));

        // A pitch curve replaced from outside shows up; the volume curve is untouched
        auto replaced = EnvelopeCurve::makeFlat (0.5f);
        plugin.updateCurve (replaced, CurveTarget::pitch);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        CHECK (curveEditor->getCurve() == replaced);
        CHECK (plugin.getCurve (CurveTarget::volume) == EnvelopeCurve {});

        curveEditor->getToolbar().setTarget (CurveTarget::volume);
        CHECK (curveEditor->getCurve() == EnvelopeCurve {});
    });
}
