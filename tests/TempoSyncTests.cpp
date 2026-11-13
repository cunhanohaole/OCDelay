#include <TempoSync.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
    int indexOf (const juce::String& name)
    {
        return TempoSync::getDivisionNames().indexOf (name);
    }
}

using Catch::Matchers::WithinAbs;

TEST_CASE ("Divisions convert to milliseconds at the given tempo", "[TempoSync]")
{
    CHECK_THAT (TempoSync::divisionToMs (indexOf ("1/4"), 120.0), WithinAbs (500.0, 1.0e-6));
    CHECK_THAT (TempoSync::divisionToMs (indexOf ("1/8"), 120.0), WithinAbs (250.0, 1.0e-6));
    CHECK_THAT (TempoSync::divisionToMs (indexOf ("1/8D"), 120.0), WithinAbs (375.0, 1.0e-6));
    CHECK_THAT (TempoSync::divisionToMs (indexOf ("1/16T"), 120.0), WithinAbs (250.0 / 3.0, 1.0e-6));
    CHECK_THAT (TempoSync::divisionToMs (indexOf ("1/1"), 60.0), WithinAbs (4000.0, 1.0e-6));
}

TEST_CASE ("Divisions run from shortest to longest", "[TempoSync]")
{
    for (size_t i = 1; i < TempoSync::divisions.size(); ++i)
        CHECK (TempoSync::divisions[i].wholeNotes > TempoSync::divisions[i - 1].wholeNotes);
}

TEST_CASE ("Default division matches the ms default at the fallback tempo", "[TempoSync]")
{
    CHECK (juce::String (TempoSync::divisions[(size_t) TempoSync::defaultDivisionIndex].name) == "1/8D");
    CHECK_THAT (TempoSync::divisionToMs (TempoSync::defaultDivisionIndex, TempoSync::fallbackBpm), WithinAbs (375.0, 1.0e-6));
}

TEST_CASE ("Out-of-range division indices are clamped", "[TempoSync]")
{
    CHECK_THAT (TempoSync::divisionToMs (-5, 120.0), WithinAbs (TempoSync::divisionToMs (0, 120.0), 1.0e-9));
    CHECK_THAT (TempoSync::divisionToMs (999, 120.0), WithinAbs (TempoSync::divisionToMs (indexOf ("1/1"), 120.0), 1.0e-9));
}
