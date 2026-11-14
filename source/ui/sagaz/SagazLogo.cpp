#include "SagazLogo.h"
#include "SagazTheme.h"

namespace
{
    // The design's SVG viewBox ("-6 -1 135 30"): every path below is in these units
    const juce::Rectangle<float> kViewBox { -6.0f, -1.0f, 135.0f, 30.0f };

    // The shear pivots on the lockup's vertical centre, so the letters lean without drifting sideways
    constexpr float kShearDegrees = 18.0f;
    constexpr float kShearPivotY = 14.0f;

    const char* const kMarkHooks[] { "M0 0H20V6H6V13H0Z", "M20 28H0V22H14V15H20Z" };

    struct Glyph
    {
        const char* pathData;
        bool evenOdd; // A's counter is a hole
    };

    const Glyph kS { "M0 0H18V4H4V10H18V24H0V20H14V14H0Z", false };
    const Glyph kA { "M0 24V0H18V24H14V15H4V24Z M4 4V11H14V4Z", true };
    const Glyph kG { "M18 0V4H4V20H14V15H10V11H18V24H0V0Z", false };
    const Glyph kZ { "M0 0H18V4L5 20H18V24H0V20L13 4H0Z", false };
    const Glyph* const kSagaz[] { &kS, &kA, &kG, &kA, &kZ };

    // Letters are drawn at 3/4 size, 19.5 units apart, starting right of the mark
    constexpr float kLetterScale = 0.75f;
    constexpr float kFirstLetterX = 31.0f;
    constexpr float kLetterAdvance = 19.5f;
    constexpr float kLetterY = 5.0f;

    juce::AffineTransform shear()
    {
        const auto shearX = -std::tan (juce::degreesToRadians (kShearDegrees));
        return juce::AffineTransform::translation (0.0f, -kShearPivotY)
            .followedBy (juce::AffineTransform::shear (shearX, 0.0f))
            .followedBy (juce::AffineTransform::translation (0.0f, kShearPivotY));
    }

    juce::Path makeMark()
    {
        juce::Path mark;
        for (const auto* hook : kMarkHooks)
            mark.addPath (juce::Drawable::parseSVGPath (hook));
        mark.applyTransform (shear());
        return mark;
    }

    // Each letter is filled on its own, so A's even-odd rule can't punch holes in its neighbours
    juce::Path makeLetter (const Glyph& glyph, int index)
    {
        auto letter = juce::Drawable::parseSVGPath (glyph.pathData);
        letter.setUsingNonZeroWinding (! glyph.evenOdd);
        letter.applyTransform (juce::AffineTransform::scale (kLetterScale)
                                   .translated (kFirstLetterX + (float) index * kLetterAdvance, kLetterY)
                                   .followedBy (shear()));
        return letter;
    }

    std::array<juce::Path, std::size (kSagaz)> makeLetters()
    {
        std::array<juce::Path, std::size (kSagaz)> letters;
        for (size_t i = 0; i < letters.size(); ++i)
            letters[i] = makeLetter (*kSagaz[i], (int) i);
        return letters;
    }
}

SagazLogo::SagazLogo()
    : mark (makeMark()),
      letters (makeLetters())
{
    setInterceptsMouseClicks (false, false);
    setTitle ("Sagaz");
}

int SagazLogo::getWidthForHeight (int height)
{
    return (int) std::ceil ((float) height * kViewBox.getWidth() / kViewBox.getHeight());
}

juce::AffineTransform SagazLogo::getDesignToComponentTransform() const
{
    // Scaled by height only, so the lockup never stretches
    return juce::AffineTransform::translation (-kViewBox.getX(), -kViewBox.getY())
        .scaled ((float) getHeight() / kViewBox.getHeight());
}

void SagazLogo::paint (juce::Graphics& g)
{
    const auto toComponent = getDesignToComponentTransform();

    g.setColour (SagazTheme::Colours::accent);
    g.fillPath (mark, toComponent);

    g.setColour (SagazTheme::Colours::textPrimary);
    for (const auto& letter : letters)
        g.fillPath (letter, toComponent);
}
