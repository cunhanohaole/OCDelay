#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <span>

// One breakpoint of the volume curve. x is the position in the tail (0 = hit, 1 = end of
// Length), y is the volume (0-1). curvature bends the segment that starts at this point.
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

// Editable volume envelope over the repeater's tail. Message thread only: the audio thread
// reads a table filled from it (see fillTable), never the curve itself.
//
// Invariants: at least two points, the first at x = 0 and the last at x = 1, points sorted by
// x, every value clamped to its range.
class VolumeCurve
{
public:
    static constexpr size_t kMaxPoints = 64;
    static constexpr float kMaxCurvature = 1.0f;

    VolumeCurve(); // straight ramp from full volume to silence

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
    static VolumeCurve fromValueTree (const juce::ValueTree& tree); // invalid tree -> default curve

    static const juce::Identifier treeType;

    bool operator== (const VolumeCurve&) const = default;

private:
    static float shapeSegment (float t, float curvature);
    static bool isValid (const std::vector<CurvePoint>& candidate);
    bool isEndPoint (int index) const;

    std::vector<CurvePoint> points;
};
