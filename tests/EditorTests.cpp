#include "helpers/test_helpers.h"
#include <Parameters.h>
#include <PluginEditor.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
    // Knobs and switches, excluding the debug-only inspector button
    juce::Array<juce::Component*> getControls (juce::Component& editor)
    {
        juce::Array<juce::Component*> controls;

        for (auto* child : editor.getChildren())
            if (dynamic_cast<juce::TextButton*> (child) == nullptr)
                controls.add (child);

        return controls;
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

        // Delay Time, Division (hidden), Feedback, Mix, Sync, Ping-Pong
        CHECK (controls.size() == 6);
        CHECK (countVisible (controls) == 5);

        for (auto* control : controls)
        {
            CHECK (bounds.contains (control->getBounds()));
            CHECK (control->getY() >= stripTop);
        }
    });
}

TEST_CASE ("Sync swaps the Delay Time knob for the Division knob", "[editor]")
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

        CHECK (isSliderShownFor ("Delay Time").endsWith ("ms"));

        syncParameter->setValueNotifyingHost (1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        CHECK (isSliderShownFor ("Delay Time") == "1/4");

        syncParameter->setValueNotifyingHost (0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        CHECK (isSliderShownFor ("Delay Time").endsWith ("ms"));
    });
}
