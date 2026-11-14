#include "CurveEditor.h"
#include "../Parameters.h"
#include "../dsp/SpeedMapping.h"
#include "sagaz/SagazTheme.h"

namespace
{
    namespace Colours = SagazTheme::Colours;

    // Panel layout (px)
    constexpr int kPanelInset = 8;
    constexpr int kToolbarTop = 7;
    constexpr int kToolbarHeight = 26;
    constexpr int kPlotTop = 40;
    constexpr int kPlotBottomInset = 10;
    constexpr int kPlotInset = 8;

    constexpr float kMarkerHeight = 9.0f;
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

// Panel insets from the UI system: the top bar, then the plot box, then the plot inside it
void CurveEditor::resized()
{
    toolbar.setBounds (kPanelInset, kToolbarTop, getWidth() - 2 * kPanelInset, kToolbarHeight);

    plotFrame = getLocalBounds().toFloat().withTrimmedLeft ((float) kPanelInset).withTrimmedRight ((float) kPanelInset + 2.0f).withTrimmedTop ((float) kPlotTop).withTrimmedBottom ((float) kPlotBottomInset);
    plotArea = plotFrame.reduced ((float) kPlotInset);
}

//==============================================================================
void CurveEditor::paint (juce::Graphics& g)
{
    drawFrames (g);
    drawGrid (g);
    drawRepeatMarkers (g);
    drawCurve (g);
    drawBarNumbers (g, drawAxisLabels (g));
    drawPlayhead (g);
    drawPoints (g);
    drawDragReadout (g);
}

// The panel, and the inset box the plot sits in
void CurveEditor::drawFrames (juce::Graphics& g) const
{
    const auto panel = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (panel, SagazTheme::Radius::large);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (panel, SagazTheme::Radius::large, SagazTheme::Stroke::hair);

    const auto frame = plotFrame.reduced (0.5f);
    g.setColour (Colours::inset);
    g.fillRoundedRectangle (frame, SagazTheme::Radius::medium);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (frame, SagazTheme::Radius::medium, SagazTheme::Stroke::hair);
}

// Bar, beat and 16th lines; 16ths drop out below 5 px apart, beats below 4 px
void CurveEditor::drawGrid (juce::Graphics& g) const
{
    for (const auto level : { 0.25f, 0.5f, 0.75f })
    {
        g.setColour (juce::exactlyEqual (level, 0.5f) ? Colours::gridBeat : Colours::gridSub);
        g.drawHorizontalLine (juce::roundToInt (toScreen (0.0f, level).y), plotArea.getX(), plotArea.getRight());
    }

    const auto totalBeats = getTotalBeats();
    const auto beatsPerBar = processor.getBeatsPerBar();
    const auto beatWidth = plotArea.getWidth() / totalBeats;
    const auto sixteenthWidth = beatWidth / 4.0f;
    const auto numSixteenths = (int) std::ceil (totalBeats * 4.0f - 0.001f);

    for (int i = 1; i < numSixteenths; ++i)
    {
        const auto isBeat = i % 4 == 0;
        const auto isBar = isBeat && std::fmod ((float) (i / 4), beatsPerBar) < 0.001f;
        if ((!isBeat && sixteenthWidth < 5.0f) || (!isBar && beatWidth < 4.0f))
            continue;

        g.setColour (isBar ? Colours::gridBar : (isBeat ? Colours::gridBeat : Colours::gridSub));
        g.drawVerticalLine (juce::roundToInt (plotArea.getX() + sixteenthWidth * (float) i), plotArea.getY(), plotArea.getBottom());
    }
}

// "2", "3", ... at the top of each bar, when bars are wide enough and clear of the axis label
void CurveEditor::drawBarNumbers (juce::Graphics& g, float clearOfX) const
{
    const auto numBars = processor.getLengthBars();
    if (plotArea.getWidth() / (float) numBars < 22.0f)
        return;

    g.setColour (Colours::textDisabled);
    g.setFont (fonts->get (SagazTextStyle::axis));

    for (int bar = 1; bar < numBars; ++bar)
    {
        const auto x = toScreen ((float) bar / (float) numBars, 0.0f).x + 4.0f;
        if (x > clearOfX)
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x, plotArea.getY() + 2.0f, 24.0f, 11.0f), juce::Justification::centredLeft, false);
    }
}

// Top and bottom of the y-axis: 100 % / 0 % on the Volume tab, end / start rate on the Speed tab
float CurveEditor::drawAxisLabels (juce::Graphics& g) const
{
    const auto speed = toolbar.getTarget() == CurveTarget::speed;
    const auto topRight = drawAxisLabel (g, speed ? getEndRateLabel() : "100 %", plotArea.getY() + 12.0f);
    drawAxisLabel (g, speed ? getStartRateLabel() : "0 %", plotArea.getBottom() - 14.0f);
    return topRight;
}

// Upper-case text on an inset-coloured plate; returns the plate's right edge
float CurveEditor::drawAxisLabel (juce::Graphics& g, const juce::String& text, float baselineY) const
{
    const auto font = fonts->get (SagazTextStyle::axis);
    const auto upper = text.toUpperCase();
    const auto plate = juce::Rectangle<float> (plotArea.getX() + 3.0f, baselineY - 9.0f, juce::GlyphArrangement::getStringWidth (font, upper) + 8.0f, 13.0f);

    g.setColour (Colours::inset.withAlpha (0.9f));
    g.fillRoundedRectangle (plate, SagazTheme::Radius::small);
    g.setColour (Colours::textSecondary);
    g.setFont (font);
    g.drawText (upper, plate.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
    return plate.getRight();
}

// Walks the repeat starts the speed curve produces, as the engine does
void CurveEditor::drawRepeatMarkers (juce::Graphics& g) const
{
    const auto startFraction = getIntervalFraction (processor.getRepeatIntervalMs());
    const auto endFraction = getIntervalFraction (processor.getEndIntervalMs());
    if (startFraction <= 0.0f || endFraction <= 0.0f)
        return;

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

// A 1 px tick per repeat; repeats closer together than the minimum spacing fill a band instead
void CurveEditor::drawRepeatMark (juce::Graphics& g, float screenX, float previousScreenX) const
{
    const auto top = plotArea.getBottom() - kMarkerHeight;

    if (previousScreenX >= 0.0f && screenX - previousScreenX < kMinMarkerSpacing)
    {
        g.setColour (Colours::marker.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (previousScreenX, top, juce::jmax (screenX, previousScreenX + 1.0f), plotArea.getBottom()));
    }
    else
    {
        g.setColour (Colours::marker.withAlpha (0.7f));
        g.fillRect (juce::Rectangle<float> (std::round (screenX), top, 1.0f, kMarkerHeight));
    }
}

// Volume in the accent colour, speed in the secondary accent, each with a soft fill below
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

    juce::Path fill (path);
    fill.lineTo (plotArea.getRight(), plotArea.getBottom());
    fill.lineTo (plotArea.getX(), plotArea.getBottom());
    fill.closeSubPath();
    g.setColour (toolbar.getTarget() == CurveTarget::speed ? Colours::accent2Soft : Colours::accentSoft);
    g.fillPath (fill);

    g.setColour (getCurveColour());
    g.strokePath (path, juce::PathStrokeType (SagazTheme::Stroke::curve, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void CurveEditor::drawPoints (juce::Graphics& g) const
{
    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const auto& point = curve.getPoints()[(size_t) i];
        drawPoint (g, toScreen (point.x, point.y), i);
    }
}

// Normal: small and hollow. Hovered: filled with a halo. Dragged: larger, light, wider halo.
void CurveEditor::drawPoint (juce::Graphics& g, juce::Point<float> centre, int index) const
{
    const auto colour = getCurveColour();
    const auto dragged = index == draggedPoint;
    const auto hovered = index == hoveredPoint && draggedPoint < 0;
    const auto circle = [centre] (float radius) { return juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre); };

    if (dragged || hovered)
    {
        g.setColour (colour.withAlpha (dragged ? 0.22f : 0.16f));
        g.fillEllipse (circle (dragged ? 11.0f : 9.0f));
    }

    const auto radius = dragged ? 5.0f : (hovered ? 4.5f : 3.5f);
    g.setColour (dragged ? Colours::textPrimary : (hovered ? colour : Colours::inset));
    g.fillEllipse (circle (radius));
    g.setColour (colour);
    g.drawEllipse (circle (radius), dragged ? 2.0f : 1.5f);
}

// While dragging: the point's value and position in a tag above it (below it near the top)
void CurveEditor::drawDragReadout (juce::Graphics& g) const
{
    if (!juce::isPositiveAndBelow (draggedPoint, curve.getNumPoints()))
        return;

    const auto& point = curve.getPoints()[(size_t) draggedPoint];
    const auto centre = toScreen (point.x, point.y);
    const auto text = getReadoutText (point);
    const auto font = fonts->get (SagazTextStyle::value);
    const auto width = juce::GlyphArrangement::getStringWidth (font, text) + 14.0f;

    const auto left = juce::jlimit (plotArea.getX(), plotArea.getRight() - width, centre.x - width * 0.5f);
    const auto top = centre.y - 32.0f < plotArea.getY() ? centre.y + 14.0f : centre.y - 32.0f;
    const auto tag = juce::Rectangle<float> (left, top, width, 18.0f);

    g.setColour (Colours::surface);
    g.fillRoundedRectangle (tag, SagazTheme::Radius::small);
    g.setColour (Colours::borderStrong);
    g.drawRoundedRectangle (tag.reduced (0.5f), SagazTheme::Radius::small, SagazTheme::Stroke::control);
    g.setColour (Colours::textPrimary);
    g.setFont (font);
    g.drawText (text, tag, juce::Justification::centred, false);
}

// A line with a small cap at the top
void CurveEditor::drawPlayhead (juce::Graphics& g) const
{
    if (shownPlayhead < 0.0f)
        return;

    const auto x = toScreen (shownPlayhead, 0.0f).x;
    g.setColour (Colours::playhead);
    g.drawLine (x, plotArea.getY(), x, plotArea.getBottom(), SagazTheme::Stroke::playhead);

    juce::Path cap;
    cap.addTriangle (x - 4.0f, plotArea.getY(), x + 4.0f, plotArea.getY(), x, plotArea.getY() + 5.0f);
    g.fillPath (cap);
}

juce::Colour CurveEditor::getCurveColour() const
{
    return toolbar.getTarget() == CurveTarget::speed ? Colours::accent2 : Colours::accent;
}

// "80 % · bar 2.25" on the Volume tab; "x2.00 · bar 2.25" (repeat speed vs the start) on the Speed tab
juce::String CurveEditor::getReadoutText (const CurvePoint& point) const
{
    const auto bar = juce::String (1.0f + point.x * (float) processor.getLengthBars(), 2);

    if (toolbar.getTarget() == CurveTarget::volume)
        return juce::String (juce::roundToInt (point.y * 100.0f)) + " % " + juce::String::fromUTF8 ("\xc2\xb7") + " bar " + bar;

    const auto startMs = processor.getRepeatIntervalMs();
    const auto intervalMs = SpeedMapping::intervalAt (startMs, processor.getEndIntervalMs(), point.y);
    const auto speed = intervalMs > 0.0 ? startMs / intervalMs : 1.0;
    return juce::String::fromUTF8 ("\xc3\x97") + juce::String (speed, 2) + " " + juce::String::fromUTF8 ("\xc2\xb7") + " bar " + bar;
}

//==============================================================================
void CurveEditor::mouseMove (const juce::MouseEvent& event)
{
    const auto point = findPointAt (event.position);
    if (point != hoveredPoint)
    {
        hoveredPoint = point;
        repaint();
    }

    if (point >= 0)
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else if (findSegmentAt (event.position) >= 0)
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void CurveEditor::mouseExit (const juce::MouseEvent&)
{
    hoveredPoint = -1;
    repaint();
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
