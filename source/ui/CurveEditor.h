#pragma once

#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Draws and edits the volume curve over the repeater's Length.
//   drag a point            move it (snaps to beats when Snap is on)
//   double-click empty      add a point
//   right-click a point     delete it
//   drag a segment up/down  bend it
// Edits go straight to the processor, which publishes them to the audio thread.
class CurveEditor : public juce::Component, private juce::Timer
{
public:
    explicit CurveEditor (PluginProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    const VolumeCurve& getCurve() const { return curve; }
    juce::Rectangle<float> getPlotArea() const { return plotArea; }
    juce::Point<float> toScreen (float x, float y) const;

private:
    void timerCallback() override;

    // Coordinates
    float toCurveX (float screenX) const;
    float toCurveY (float screenY) const;
    float snapX (float x) const;
    int findPointAt (juce::Point<float> position) const;
    int findSegmentAt (juce::Point<float> position) const;

    // Editing
    void commitCurve();
    void bendSegment (float mouseY);

    // Drawing
    void drawGrid (juce::Graphics&) const;
    void drawRepeatMarkers (juce::Graphics&) const;
    void drawCurve (juce::Graphics&) const;
    void drawPoints (juce::Graphics&) const;
    void drawPlayhead (juce::Graphics&) const;

    float getTotalBeats() const;
    float getSliceFraction() const; // slice length as a fraction of Length

    PluginProcessor& processor;
    VolumeCurve curve; // local copy, kept in sync with the processor
    int shownCurveVersion = -1;

    juce::ToggleButton snapButton { "Snap" };
    juce::Rectangle<float> plotArea;

    int draggedPoint = -1;
    int bentSegment = -1;
    float bendStartCurvature = 0.0f;
    float bendStartY = 0.0f;

    // Last values drawn, so the timer only repaints when something changed
    float shownPlayhead = -1.0f;
    float shownSliceFraction = 0.0f;
    float shownTotalBeats = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveEditor)
};
