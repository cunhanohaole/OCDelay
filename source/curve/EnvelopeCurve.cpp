#include "EnvelopeCurve.h"

namespace
{
    const juce::Identifier pointType { "Point" };
    const juce::Identifier xProperty { "x" };
    const juce::Identifier yProperty { "y" };
    const juce::Identifier curvatureProperty { "curvature" };

    float clampUnit (float value)
    {
        return juce::jlimit (0.0f, 1.0f, value);
    }
}

const juce::Identifier EnvelopeCurve::treeType { "Curve" };

EnvelopeCurve::EnvelopeCurve()
    : points { { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }
{
}

EnvelopeCurve EnvelopeCurve::makeFlat (float y)
{
    EnvelopeCurve curve;
    curve.points = { { 0.0f, clampUnit (y), 0.0f }, { 1.0f, clampUnit (y), 0.0f } };
    return curve;
}

int EnvelopeCurve::addPoint (float x, float y)
{
    if (points.size() >= kMaxPoints)
        return -1;

    const CurvePoint point { clampUnit (x), clampUnit (y), 0.0f };

    // Insert after any points at the same x, but never after the last point
    auto position = std::upper_bound (points.begin() + 1, points.end() - 1, point.x, [] (float value, const CurvePoint& p) { return value < p.x; });

    // insert() may reallocate, so measure from begin() only after it returns
    const auto inserted = points.insert (position, point);
    return (int) std::distance (points.begin(), inserted);
}

void EnvelopeCurve::movePoint (int index, float x, float y)
{
    if (!juce::isPositiveAndBelow (index, getNumPoints()))
        return;

    auto& point = points[(size_t) index];
    point.y = clampUnit (y);

    if (isEndPoint (index))
        return;

    const auto minX = points[(size_t) index - 1].x;
    const auto maxX = points[(size_t) index + 1].x;
    point.x = juce::jlimit (minX, maxX, x);
}

bool EnvelopeCurve::removePoint (int index)
{
    if (!juce::isPositiveAndBelow (index, getNumPoints()) || isEndPoint (index))
        return false;

    points.erase (points.begin() + index);
    return true;
}

void EnvelopeCurve::setCurvature (int segmentIndex, float curvature)
{
    if (juce::isPositiveAndBelow (segmentIndex, getNumPoints() - 1))
        points[(size_t) segmentIndex].curvature = juce::jlimit (-kMaxCurvature, kMaxCurvature, curvature);
}

float EnvelopeCurve::evaluate (float x) const
{
    x = clampUnit (x);

    // First point whose x is beyond the position; the segment starts one before it
    auto next = std::upper_bound (points.begin() + 1, points.end() - 1, x, [] (float value, const CurvePoint& p) { return value < p.x; });
    const auto& end = *next;
    const auto& start = *(next - 1);

    const auto width = end.x - start.x;
    if (width <= 0.0f)
        return end.y;

    const auto t = (x - start.x) / width;
    return start.y + (end.y - start.y) * shapeSegment (t, start.curvature);
}

void EnvelopeCurve::fillTable (std::span<float> table) const
{
    if (table.empty())
        return;

    const auto lastIndex = (float) juce::jmax ((size_t) 1, table.size() - 1);

    for (size_t i = 0; i < table.size(); ++i)
        table[i] = evaluate ((float) i / lastIndex);
}

juce::ValueTree EnvelopeCurve::toValueTree() const
{
    juce::ValueTree tree { treeType };

    for (const auto& point : points)
    {
        juce::ValueTree child { pointType };
        child.setProperty (xProperty, point.x, nullptr);
        child.setProperty (yProperty, point.y, nullptr);
        child.setProperty (curvatureProperty, point.curvature, nullptr);
        tree.appendChild (child, nullptr);
    }

    return tree;
}

EnvelopeCurve EnvelopeCurve::fromValueTree (const juce::ValueTree& tree, const EnvelopeCurve& fallback)
{
    auto curve = fallback;

    if (!tree.hasType (treeType))
        return curve;

    std::vector<CurvePoint> loaded;

    for (const auto& child : tree)
    {
        if (!child.hasType (pointType) || loaded.size() >= kMaxPoints)
            return curve;

        loaded.push_back ({ clampUnit ((float) child[xProperty]),
            clampUnit ((float) child[yProperty]),
            juce::jlimit (-kMaxCurvature, kMaxCurvature, (float) child[curvatureProperty]) });
    }

    if (isValid (loaded))
        curve.points = std::move (loaded);

    return curve;
}

// Power curve: c = 0 is linear; positive c holds near the start value longer, negative c
// moves towards the end value sooner
float EnvelopeCurve::shapeSegment (float t, float curvature)
{
    if (juce::approximatelyEqual (curvature, 0.0f))
        return t;

    return std::pow (t, std::exp2 (3.0f * curvature));
}

bool EnvelopeCurve::isValid (const std::vector<CurvePoint>& candidate)
{
    if (candidate.size() < 2)
        return false;

    if (!juce::exactlyEqual (candidate.front().x, 0.0f) || !juce::exactlyEqual (candidate.back().x, 1.0f))
        return false;

    return std::is_sorted (candidate.begin(), candidate.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });
}

bool EnvelopeCurve::isEndPoint (int index) const
{
    return index == 0 || index == getNumPoints() - 1;
}
