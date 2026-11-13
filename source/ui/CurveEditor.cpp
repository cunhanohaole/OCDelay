#include "CurveEditor.h"
#include "../Parameters.h"
#include "../dsp/SpeedMapping.h"

namespace
{
    constexpr float kPointRadius = 4.5f;
    constexpr float kPointHitRadius = 9.0f;
    constexpr float kSegmentHitDistance = 10.0f;
    constexpr float kMinMarkerSpacing = 4.0f; // px; denser repeat markers merge into a band
    constexpr int kMaxMarkers = 20000; // 5 ms repeats over 32 slow bars stay below this
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
    drawSpeedLabels (g);
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

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (juce::roundToInt (plotArea.getCentreY()), plotArea.getX(), plotArea.getRight());
}

// The end rate at the top, the start rate at the bottom
void CurveEditor::drawSpeedLabels (juce::Graphics& g) const
{
    if (toolbar.getTarget() != CurveTarget::speed)
        return;

    const auto labelArea = plotArea.reduced (kPointHitRadius + 4.0f, 2.0f);
    const auto labelHeight = 14.0f;

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (12.0f);
    g.drawText (getEndRateLabel(), labelArea.withHeight (labelHeight), juce::Justification::topLeft);
    g.drawText (getStartRateLabel(), labelArea.withTrimmedTop (labelArea.getHeight() - labelHeight), juce::Justification::bottomLeft);
}

// Walks the repeat starts the speed curve produces, as the engine does
void CurveEditor::drawRepeatMarkers (juce::Graphics& g) const
{
    const auto startFraction = getIntervalFraction (processor.getRepeatIntervalMs());
    const auto endFraction = getIntervalFraction (processor.getEndIntervalMs());
    if (startFraction <= 0.0f || endFraction <= 0.0f)
        return;

    g.setColour (getLookAndFeel().findColour (juce::Slider::thumbColourId).withAlpha (0.25f));

    auto previousX = -1.0f;
    auto fraction = getNextRepeatFraction (0.0, startFraction, endFraction);

    for (int count = 0; fraction < 1.0 && count < kMaxMarkers; ++count)
    {
        const auto screenX = toScreen ((float) fraction, 0.0f).x;
        drawRepeatMark (g, screenX, previousX);
        previousX = screenX;
        fraction = getNextRepeatFraction (fraction, startFraction, endFraction);
    }
}

// A line per repeat; repeats closer together than the minimum spacing fill a band instead
void CurveEditor::drawRepeatMark (juce::Graphics& g, float screenX, float previousScreenX) const
{
    const auto top = plotArea.getBottom() - plotArea.getHeight() * 0.08f;

    if (previousScreenX >= 0.0f && screenX - previousScreenX < kMinMarkerSpacing)
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (previousScreenX, top, juce::jmax (screenX, previousScreenX + 1.0f), plotArea.getBottom()));
    else
        g.drawLine (screenX, top, screenX, plotArea.getBottom(), 1.5f);
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

    const auto baselineY = plotArea.getBottom();
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
        curve.movePoint (draggedPoint, snapX (toCurveX (event.position.x)), toCurveY (event.position.y));
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

    if (curve.addPoint (snapX (toCurveX (event.position.x)), toCurveY (event.position.y)) >= 0)
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
    const auto sliceFraction = getIntervalFraction (processor.getRepeatIntervalMs());
    const auto endFraction = getIntervalFraction (processor.getEndIntervalMs());
    const auto totalBeats = getTotalBeats();
    const auto rateLabels = getStartRateLabel() + "|" + getEndRateLabel();

    if (!juce::exactlyEqual (playhead, shownPlayhead) || !juce::exactlyEqual (sliceFraction, shownSliceFraction)
        || !juce::exactlyEqual (endFraction, shownEndFraction) || !juce::exactlyEqual (totalBeats, shownTotalBeats)
        || rateLabels != shownRateLabels)
    {
        shownPlayhead = playhead;
        shownSliceFraction = sliceFraction;
        shownEndFraction = endFraction;
        shownTotalBeats = totalBeats;
        shownRateLabels = rateLabels;
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

float CurveEditor::getIntervalFraction (float intervalMs) const
{
    const auto msPerBeat = 60000.0f / juce::jmax (1.0f, processor.getHostBpm());
    const auto lengthMs = getTotalBeats() * msPerBeat;
    return intervalMs / lengthMs;
}

// The next repeat starts one interval later, the interval read from the speed curve here
double CurveEditor::getNextRepeatFraction (double fraction, float startFraction, float endFraction) const
{
    const auto curveValue = processor.getCurve (CurveTarget::speed).evaluate ((float) fraction);
    return fraction + SpeedMapping::intervalAt (startFraction, endFraction, curveValue);
}

namespace
{
    juce::String getParameterText (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id)
    {
        return apvts.getParameter (id.getParamID())->getCurrentValueAsText();
    }

    bool isSwitchOn (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id)
    {
        return apvts.getRawParameterValue (id.getParamID())->load() >= 0.5f;
    }
}

juce::String CurveEditor::getStartRateLabel() const
{
    auto& apvts = processor.getAPVTS();
    const auto sync = isSwitchOn (apvts, ParamIDs::sync);

    if (isSwitchOn (apvts, ParamIDs::freeRate))
        return getParameterText (apvts, sync ? ParamIDs::rateDivision : ParamIDs::rate);

    return getParameterText (apvts, sync ? ParamIDs::division : ParamIDs::delayTime);
}

juce::String CurveEditor::getEndRateLabel() const
{
    auto& apvts = processor.getAPVTS();
    if (isSwitchOn (apvts, ParamIDs::maxSpeed))
        return "Max";

    return getParameterText (apvts, isSwitchOn (apvts, ParamIDs::sync) ? ParamIDs::endRateDivision : ParamIDs::endRate);
}
