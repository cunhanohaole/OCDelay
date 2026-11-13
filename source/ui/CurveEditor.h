#pragma once

#include "../PluginProcessor.h"
#include "CurveToolbar.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Draws and edits the volume and speed curves over the repeater's Length (one tab each).
//   drag a point            move it (Snap: to beats)
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

    const EnvelopeCurve& getCurve() const { return curve; } // the shown tab's curve
    CurveToolbar& getToolbar() { return toolbar; }
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
    void showTargetCurve();
    void bendSegment (float mouseY);

    // Drawing
    void drawGrid (juce::Graphics&) const;
    void drawSpeedLabels (juce::Graphics&) const;
    void drawRepeatMark (juce::Graphics&, float screenX, float previousScreenX) const;
    void drawRepeatMarkers (juce::Graphics&) const;
    void drawCurve (juce::Graphics&) const;
    void drawPoints (juce::Graphics&) const;
    void drawPlayhead (juce::Graphics&) const;

    float getTotalBeats() const;
    float getIntervalFraction (float intervalMs) const; // an interval as a fraction of Length
    double getNextRepeatFraction (double fraction, float startFraction, float endFraction) const;

    // Speed-curve axis labels: the start rate (Slice or Rate) and the end rate (End Rate or Max)
    juce::String getStartRateLabel() const;
    juce::String getEndRateLabel() const;

    PluginProcessor& processor;
    EnvelopeCurve curve; // local copy, kept in sync with the processor
    int shownCurveVersion = -1;

    CurveToolbar toolbar;
    juce::Rectangle<float> plotArea;

    int draggedPoint = -1;
    int bentSegment = -1;
    float bendStartCurvature = 0.0f;
    float bendStartY = 0.0f;

    // Last values drawn, so the timer only repaints when something changed
    float shownPlayhead = -1.0f;
    float shownSliceFraction = 0.0f;
    float shownEndFraction = 0.0f;
    float shownTotalBeats = 0.0f;
    juce::String shownRateLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveEditor)
};
