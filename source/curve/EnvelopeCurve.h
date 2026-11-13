#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <span>

// One breakpoint of an envelope curve. x is the position in the tail (0 = hit, 1 = end of
// Length), y is the normalised value (0-1). curvature bends the segment that starts here.
struct CurvePoint
{
    float x = 0.0f;
    float y = 0.0f;
    float curvature = 0.0f; // -1..1, 0 = straight line

    bool operator== (const CurvePoint& other) const
    {
        return juce::exactlyEqual (x, other.x) && juce::exactlyEqual (y, other.y) && juce::exactlyEqual (curvature, other.curvature);
    }
};

// Editable envelope over the repeater's tail, used for both volume and speed. Message thread
// only: the audio thread reads a table filled from it (see fillTable), never the curve itself.
//
// Invariants: at least two points, the first at x = 0 and the last at x = 1, points sorted by
// x, every value clamped to its range.
class EnvelopeCurve
{
public:
    static constexpr size_t kMaxPoints = 64;
    static constexpr float kMaxCurvature = 1.0f;

    EnvelopeCurve(); // straight ramp from 1 down to 0 (the default volume curve)
    static EnvelopeCurve makeFlat (float y); // a constant value (the default speed curve)

    const std::vector<CurvePoint>& getPoints() const { return points; }
    int getNumPoints() const { return (int) points.size(); }

    // Returns the index of the new point, or -1 if the curve is full
    int addPoint (float x, float y);

    // x is kept between the neighbouring points so indices never change while dragging.
    // The end points only move vertically.
    void movePoint (int index, float x, float y);

    // The end points can't be removed; returns false for them
    bool removePoint (int index);

    // Bends the segment between point segmentIndex and the next one
    void setCurvature (int segmentIndex, float curvature);

    float evaluate (float x) const;
    void fillTable (std::span<float> table) const;

    juce::ValueTree toValueTree() const;
    // An invalid or missing tree gives the fallback
    static EnvelopeCurve fromValueTree (const juce::ValueTree& tree, const EnvelopeCurve& fallback = {});

    static const juce::Identifier treeType;

    bool operator== (const EnvelopeCurve&) const = default;

private:
    static float shapeSegment (float t, float curvature);
    static bool isValid (const std::vector<CurvePoint>& candidate);
    bool isEndPoint (int index) const;

    std::vector<CurvePoint> points;
};
