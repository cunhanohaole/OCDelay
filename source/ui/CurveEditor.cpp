#include "CurveEditor.h"
#include "../dsp/PitchMapping.h"

namespace
{
    constexpr float kPointRadius = 4.5f;
    constexpr float kPointHitRadius = 9.0f;
    constexpr float kSegmentHitDistance = 10.0f;
    constexpr float kMinMarkerSpacing = 3.0f; // px; denser repeat markers are skipped
    constexpr int kRefreshHz = 30;

    // Dragging the full plot height changes curvature by this much
    constexpr float kBendSensitivity = 2.0f;
}

CurveEditor::CurveEditor (PluginProcessor& p)
    : processor (p),
      toolbar (p.getAPVTS())
{
    toolbar.onTargetChange = [this] { showTargetCurve(); };
    addAndMakeVisible (toolbar);

    setTitle ("Curves"); // accessibility name
    timerCallback(); // pick up the processor's curve before the first paint
    startTimerHz (kRefreshHz);
}

void CurveEditor::resized()
{
    auto area = getLocalBounds();
    toolbar.setBounds (area.removeFromTop (proportionOfHeight (0.12f)));

    plotArea = area.reduced (proportionOfWidth (0.015f), proportionOfHeight (0.03f)).toFloat();
}

//==============================================================================
void CurveEditor::paint (juce::Graphics& g)
{
    g.setColour (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.3f));
    g.fillRoundedRectangle (plotArea, 4.0f);

    drawGrid (g);
    drawPitchLabels (g);
    drawRepeatMarkers (g);
    drawCurve (g);
    drawPoints (g);
    drawPlayhead (g);
}

void CurveEditor::drawGrid (juce::Graphics& g) const
{
    const auto totalBeats = getTotalBeats();
    const auto beatsPerBar = processor.getBeatsPerBar();
    const auto beatWidth = plotArea.getWidth() / totalBeats;
    const auto showBeats = beatWidth >= 6.0f;

    for (int beat = 1; beat < (int) std::ceil (totalBeats); ++beat)
    {
        const auto isBarLine = std::fmod ((float) beat, beatsPerBar) < 0.001f;
        if (!isBarLine && !showBeats)
            continue;

        g.setColour (juce::Colours::white.withAlpha (isBarLine ? 0.18f : 0.06f));
        const auto x = plotArea.getX() + beatWidth * (float) beat;
        g.drawVerticalLine (juce::roundToInt (x), plotArea.getY(), plotArea.getBottom());
    }

    // Volume: a 50 % guide. Pitch: the "no change" line, a little brighter.
    const auto isPitch = toolbar.getTarget() == CurveTarget::pitch;
    const auto guideY = isPitch ? toScreen (0.0f, getBaseline()).y : plotArea.getCentreY();
    g.setColour (juce::Colours::white.withAlpha (isPitch ? 0.25f : 0.06f));
    g.drawHorizontalLine (juce::roundToInt (guideY), plotArea.getX(), plotArea.getRight());
}

// Semitone values at the top, bottom and "no change" line
void CurveEditor::drawPitchLabels (juce::Graphics& g) const
{
    if (toolbar.getTarget() != CurveTarget::pitch)
        return;

    const auto range = juce::roundToInt (processor.getPitchRangeSemitones());
    const auto bipolar = processor.isPitchBipolar();
    const auto labelArea = plotArea.reduced (4.0f, 2.0f);
    const auto labelHeight = 14.0f;

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (12.0f);
    g.drawText ("+" + juce::String (range) + " st", labelArea.withHeight (labelHeight), juce::Justification::topLeft);
    g.drawText (bipolar ? juce::String::fromUTF8 ("\xe2\x88\x92") + juce::String (range) + " st" : "0 st",
        labelArea.withTrimmedTop (labelArea.getHeight() - labelHeight), juce::Justification::bottomLeft);

    if (bipolar)
    {
        // Right of the first point, just above the "no change" line
        const auto zeroArea = labelArea.withTrimmedLeft (kPointHitRadius + 4.0f).withHeight (labelHeight);
        g.drawText ("0 st", zeroArea.withY (plotArea.getCentreY() - labelHeight - 1.0f), juce::Justification::left);
    }
}

void CurveEditor::drawRepeatMarkers (juce::Graphics& g) const
{
    const auto sliceFraction = getSliceFraction();
    if (sliceFraction <= 0.0f || sliceFraction * plotArea.getWidth() < kMinMarkerSpacing)
        return;

    g.setColour (getLookAndFeel().findColour (juce::Slider::thumbColourId).withAlpha (0.25f));

    for (float x = sliceFraction; x < 1.0f; x += sliceFraction)
    {
        const auto screenX = toScreen (x, 0.0f).x;
        g.drawLine (screenX, plotArea.getBottom() - plotArea.getHeight() * 0.08f, screenX, plotArea.getBottom(), 1.5f);
    }
}

void CurveEditor::drawCurve (juce::Graphics& g) const
{
    juce::Path path;
    const auto steps = juce::jmax (2, juce::roundToInt (plotArea.getWidth() / 2.0f));

    for (int i = 0; i <= steps; ++i)
    {
        const auto x = (float) i / (float) steps;
        const auto point = toScreen (x, curve.evaluate (x));
        i == 0 ? path.startNewSubPath (point) : path.lineTo (point);
    }

    const auto colour = getLookAndFeel().findColour (juce::Slider::thumbColourId);

    const auto baselineY = toScreen (0.0f, getBaseline()).y;
    juce::Path fill (path);
    fill.lineTo (plotArea.getRight(), baselineY);
    fill.lineTo (plotArea.getX(), baselineY);
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.12f));
    g.fillPath (fill);

    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (2.0f));
}

void CurveEditor::drawPoints (juce::Graphics& g) const
{
    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const auto& point = curve.getPoints()[(size_t) i];
        const auto centre = toScreen (point.x, point.y);
        const auto radius = i == draggedPoint ? kPointRadius * 1.4f : kPointRadius;

        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    }
}

void CurveEditor::drawPlayhead (juce::Graphics& g) const
{
    if (shownPlayhead < 0.0f)
        return;

    g.setColour (juce::Colours::white.withAlpha (0.6f));
    const auto x = toScreen (shownPlayhead, 0.0f).x;
    g.drawLine (x, plotArea.getY(), x, plotArea.getBottom(), 1.0f);
}

//==============================================================================
void CurveEditor::mouseMove (const juce::MouseEvent& event)
{
    if (findPointAt (event.position) >= 0)
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else if (findSegmentAt (event.position) >= 0)
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void CurveEditor::mouseDown (const juce::MouseEvent& event)
{
    const auto point = findPointAt (event.position);

    if (event.mods.isPopupMenu())
    {
        if (point >= 0 && curve.removePoint (point))
            commitCurve();
        return;
    }

    draggedPoint = point;
    bentSegment = point < 0 ? findSegmentAt (event.position) : -1;

    if (bentSegment >= 0)
    {
        bendStartCurvature = curve.getPoints()[(size_t) bentSegment].curvature;
        bendStartY = event.position.y;
    }

    repaint();
}

void CurveEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (draggedPoint >= 0)
    {
        curve.movePoint (draggedPoint, snapX (toCurveX (event.position.x)), snapY (toCurveY (event.position.y)));
        commitCurve();
    }
    else if (bentSegment >= 0)
    {
        bendSegment (event.position.y);
    }
}

void CurveEditor::mouseUp (const juce::MouseEvent&)
{
    draggedPoint = -1;
    bentSegment = -1;
    repaint();
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (!plotArea.contains (event.position) || findPointAt (event.position) >= 0)
        return;

    if (curve.addPoint (snapX (toCurveX (event.position.x)), snapY (toCurveY (event.position.y))) >= 0)
        commitCurve();
}

// Dragging up always bends the segment upwards, whichever way it slopes
void CurveEditor::bendSegment (float mouseY)
{
    const auto& start = curve.getPoints()[(size_t) bentSegment];
    const auto& end = curve.getPoints()[(size_t) bentSegment + 1];
    const auto direction = start.y >= end.y ? 1.0f : -1.0f;
    const auto dragAmount = (bendStartY - mouseY) / plotArea.getHeight();

    curve.setCurvature (bentSegment, bendStartCurvature + direction * dragAmount * kBendSensitivity);
    commitCurve();
}

void CurveEditor::commitCurve()
{
    processor.updateCurve (curve, toolbar.getTarget());
    shownCurveVersion = processor.getCurveVersion();
    repaint();
}

void CurveEditor::showTargetCurve()
{
    draggedPoint = -1;
    bentSegment = -1;
    curve = processor.getCurve (toolbar.getTarget());
    shownCurveVersion = processor.getCurveVersion();
    repaint();
}

//==============================================================================
void CurveEditor::timerCallback()
{
    // The curve can be replaced from outside, e.g. by loading a session
    if (processor.getCurveVersion() != shownCurveVersion && draggedPoint < 0 && bentSegment < 0)
    {
        showTargetCurve();
    }

    const auto playhead = processor.getPlayheadPosition();
    const auto sliceFraction = getSliceFraction();
    const auto totalBeats = getTotalBeats();
    const auto pitchRange = processor.getPitchRangeSemitones();
    const auto bipolar = processor.isPitchBipolar();

    if (!juce::exactlyEqual (playhead, shownPlayhead) || !juce::exactlyEqual (sliceFraction, shownSliceFraction)
        || !juce::exactlyEqual (totalBeats, shownTotalBeats) || !juce::exactlyEqual (pitchRange, shownPitchRange)
        || bipolar != shownBipolar)
    {
        shownPlayhead = playhead;
        shownSliceFraction = sliceFraction;
        shownTotalBeats = totalBeats;
        shownPitchRange = pitchRange;
        shownBipolar = bipolar;
        repaint();
    }
}

//==============================================================================
juce::Point<float> CurveEditor::toScreen (float x, float y) const
{
    return { plotArea.getX() + x * plotArea.getWidth(), plotArea.getBottom() - y * plotArea.getHeight() };
}

float CurveEditor::toCurveX (float screenX) const
{
    return juce::jlimit (0.0f, 1.0f, (screenX - plotArea.getX()) / plotArea.getWidth());
}

float CurveEditor::toCurveY (float screenY) const
{
    return juce::jlimit (0.0f, 1.0f, (plotArea.getBottom() - screenY) / plotArea.getHeight());
}

float CurveEditor::snapX (float x) const
{
    if (!toolbar.isSnapOn())
        return x;

    const auto totalBeats = getTotalBeats();
    return std::round (x * totalBeats) / totalBeats;
}

// Pitch tab only: whole semitones at the current Range and Bipolar setting
float CurveEditor::snapY (float y) const
{
    if (!toolbar.isSnapOn() || toolbar.getTarget() != CurveTarget::pitch)
        return y;

    const auto range = processor.getPitchRangeSemitones();
    const auto bipolar = processor.isPitchBipolar();
    const auto semitones = std::round (PitchMapping::toSemitones (y, range, bipolar));
    return juce::jlimit (0.0f, 1.0f, PitchMapping::toCurveValue (semitones, range, bipolar));
}

float CurveEditor::getBaseline() const
{
    if (toolbar.getTarget() == CurveTarget::pitch && processor.isPitchBipolar())
        return 0.5f;

    return 0.0f;
}

int CurveEditor::findPointAt (juce::Point<float> position) const
{
    for (int i = curve.getNumPoints(); --i >= 0;)
    {
        const auto& point = curve.getPoints()[(size_t) i];
        if (toScreen (point.x, point.y).getDistanceFrom (position) <= kPointHitRadius)
            return i;
    }

    return -1;
}

// The segment under the mouse, if the mouse is close to the drawn curve
int CurveEditor::findSegmentAt (juce::Point<float> position) const
{
    if (!plotArea.contains (position))
        return -1;

    const auto x = toCurveX (position.x);
    const auto curveY = toScreen (x, curve.evaluate (x)).y;
    if (std::abs (curveY - position.y) > kSegmentHitDistance)
        return -1;

    const auto& points = curve.getPoints();
    for (int i = 0; i < curve.getNumPoints() - 1; ++i)
        if (x >= points[(size_t) i].x && x <= points[(size_t) i + 1].x)
            return i;

    return -1;
}

float CurveEditor::getTotalBeats() const
{
    return juce::jmax (1.0f, (float) processor.getLengthBars() * processor.getBeatsPerBar());
}

float CurveEditor::getSliceFraction() const
{
    const auto msPerBeat = 60000.0f / juce::jmax (1.0f, processor.getHostBpm());
    const auto lengthMs = getTotalBeats() * msPerBeat;
    return processor.getRepeatIntervalMs() / lengthMs;
}
