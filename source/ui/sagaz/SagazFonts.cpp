#include "SagazFonts.h"
#include <BinaryData.h>

namespace
{
    juce::Typeface::Ptr loadTypeface (const char* data, int size)
    {
        auto typeface = juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        jassert (typeface != nullptr);
        return typeface;
    }

    struct StyleSpec
    {
        float size;
        float tracking; // fraction of the size, like CSS letter-spacing in em
    };

    StyleSpec getSpec (SagazTextStyle style)
    {
        switch (style)
        {
            case SagazTextStyle::tab: return { 12.0f, 0.0f };
            case SagazTextStyle::axis: return { 9.0f, 0.05f };
            case SagazTextStyle::title: return { 14.0f, -0.01f };
            case SagazTextStyle::label:
            case SagazTextStyle::switchLabel:
            case SagazTextStyle::value:
            case SagazTextStyle::button: break;
        }
        return { 11.0f, 0.0f };
    }
}

SagazFonts::SagazFonts()
    : medium (loadTypeface (BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize)),
      semiBold (loadTypeface (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize)),
      bold (loadTypeface (BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize))
{
}

juce::Font SagazFonts::get (SagazTextStyle style) const
{
    const auto isMedium = style == SagazTextStyle::switchLabel || style == SagazTextStyle::value;
    const auto typeface = style == SagazTextStyle::title ? bold : (isMedium ? medium : semiBold);
    const auto spec = getSpec (style);

    return juce::Font (juce::FontOptions (typeface)
                           .withPointHeight (spec.size)
                           .withKerningFactor (spec.tracking));
}
