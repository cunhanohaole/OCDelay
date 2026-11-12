#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <curve/TripleBuffer.h>
#include <curve/EnvelopeCurve.h>
#include <thread>

using Catch::Matchers::WithinAbs;

TEST_CASE ("Default curve is a straight ramp down", "[EnvelopeCurve]")
{
    EnvelopeCurve curve;

    REQUIRE (curve.getNumPoints() == 2);
    CHECK_THAT (curve.evaluate (0.0f), WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (curve.evaluate (0.25f), WithinAbs (0.75, 1.0e-6));
    CHECK_THAT (curve.evaluate (1.0f), WithinAbs (0.0, 1.0e-6));
}

TEST_CASE ("Added points are sorted and shape the curve", "[EnvelopeCurve]")
{
    EnvelopeCurve curve;
    CHECK (curve.addPoint (0.5f, 1.0f) == 1);
    CHECK (curve.addPoint (0.25f, 0.0f) == 1);

    CHECK_THAT (curve.evaluate (0.25f), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (curve.evaluate (0.5f), WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (curve.evaluate (0.375f), WithinAbs (0.5, 1.0e-6));
}

TEST_CASE ("Points stay between their neighbours while moving", "[EnvelopeCurve]")
{
    EnvelopeCurve curve;
    curve.addPoint (0.3f, 0.5f);
    curve.addPoint (0.6f, 0.5f);

    curve.movePoint (1, 0.9f, 2.0f);
    CHECK_THAT (curve.getPoints()[1].x, WithinAbs (0.6, 1.0e-6));
    CHECK_THAT (curve.getPoints()[1].y, WithinAbs (1.0, 1.0e-6));
}

TEST_CASE ("End points only move vertically and can't be removed", "[EnvelopeCurve]")
{
    EnvelopeCurve curve;
    curve.movePoint (0, 0.5f, 0.4f);
    curve.movePoint (1, 0.5f, 0.2f);

    CHECK_THAT (curve.getPoints().front().x, WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (curve.getPoints().front().y, WithinAbs (0.4, 1.0e-6));
    CHECK_THAT (curve.getPoints().back().x, WithinAbs (1.0, 1.0e-6));
    CHECK_FALSE (curve.removePoint (0));
    CHECK_FALSE (curve.removePoint (1));

    curve.addPoint (0.5f, 1.0f);
    CHECK (curve.removePoint (1));
    CHECK (curve.getNumPoints() == 2);
}

TEST_CASE ("Curve holds at most kMaxPoints", "[EnvelopeCurve]")
{
    EnvelopeCurve curve;
    for (size_t i = 2; i < EnvelopeCurve::kMaxPoints; ++i)
        REQUIRE (curve.addPoint (0.5f, 0.5f) >= 0);

    CHECK (curve.addPoint (0.5f, 0.5f) == -1);
}

TEST_CASE ("Curvature bends a segment", "[EnvelopeCurve]")
{
    EnvelopeCurve curve; // 1 -> 0

    curve.setCurvature (0, 1.0f);
    CHECK (curve.evaluate (0.5f) > 0.9f); // holds near full volume, then drops

    curve.setCurvature (0, -1.0f);
    CHECK (curve.evaluate (0.5f) < 0.1f); // drops early

    curve.setCurvature (0, 5.0f);
    CHECK_THAT (curve.getPoints()[0].curvature, WithinAbs (EnvelopeCurve::kMaxCurvature, 1.0e-6));
}

TEST_CASE ("fillTable samples the whole curve", "[EnvelopeCurve]")
{
    EnvelopeCurve curve;
    std::array<float, 5> table {};
    curve.fillTable (table);

    CHECK_THAT (table[0], WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (table[2], WithinAbs (0.5, 1.0e-6));
    CHECK_THAT (table[4], WithinAbs (0.0, 1.0e-6));
}

TEST_CASE ("Curve round-trips through a ValueTree", "[EnvelopeCurve]")
{
    EnvelopeCurve curve;
    curve.addPoint (0.4f, 0.8f);
    curve.setCurvature (1, -0.5f);

    CHECK (EnvelopeCurve::fromValueTree (curve.toValueTree()) == curve);
}

TEST_CASE ("Invalid trees fall back to the default curve", "[EnvelopeCurve]")
{
    const EnvelopeCurve defaultCurve;
    CHECK (EnvelopeCurve::fromValueTree ({}) == defaultCurve);
    CHECK (EnvelopeCurve::fromValueTree (juce::ValueTree { "Wrong" }) == defaultCurve);

    // A single point breaks the "at least two points, ends at 0 and 1" invariant
    juce::ValueTree tree { EnvelopeCurve::treeType };
    juce::ValueTree point { "Point" };
    point.setProperty ("x", 0.5f, nullptr);
    tree.appendChild (point, nullptr);
    CHECK (EnvelopeCurve::fromValueTree (tree) == defaultCurve);
}

TEST_CASE ("TripleBuffer hands the newest value to the reader", "[TripleBuffer]")
{
    TripleBuffer<int> buffer { 7 };
    CHECK (buffer.read() == 7);

    buffer.getWriteBuffer() = 1;
    buffer.publish();
    buffer.getWriteBuffer() = 2;
    buffer.publish();
    CHECK (buffer.read() == 2);
    CHECK (buffer.read() == 2); // reading again without a publish keeps the value

    buffer.getWriteBuffer() = 3;
    buffer.publish();
    CHECK (buffer.read() == 3);
}

TEST_CASE ("TripleBuffer never hands out a half-written value", "[TripleBuffer]")
{
    // Every published array is filled with one value; a torn read would mix values
    using Table = std::array<int, 256>;
    TripleBuffer<Table> buffer { Table {} };
    std::atomic<bool> done { false };

    std::thread writer ([&] {
        for (int value = 1; value <= 20000; ++value)
        {
            buffer.getWriteBuffer().fill (value);
            buffer.publish();
        }
        done = true;
    });

    bool torn = false;
    int lastSeen = 0;
    bool wentBackwards = false;
    while (!done)
    {
        const auto& table = buffer.read();
        torn |= std::any_of (table.begin(), table.end(), [&] (int v) { return v != table.front(); });
        wentBackwards |= table.front() < lastSeen;
        lastSeen = table.front();
    }
    writer.join();

    CHECK_FALSE (torn);
    CHECK_FALSE (wentBackwards);
}
