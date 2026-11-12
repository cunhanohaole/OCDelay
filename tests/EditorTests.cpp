#include "helpers/test_helpers.h"
#include <PluginEditor.h>
#include <catch2/catch_test_macros.hpp>

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
        const auto stripTop = bounds.getHeight() * 6 / 10;

        int numControls = 0;
        for (auto* child : editor->getChildren())
        {
            if (dynamic_cast<juce::TextButton*> (child) != nullptr)
                continue; // debug-only inspector button

            ++numControls;
            CHECK (bounds.contains (child->getBounds()));
            CHECK (child->getY() >= stripTop);
        }

        CHECK (numControls == 4);
    });
}
