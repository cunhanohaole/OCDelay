#include "SagazLookAndFeel.h"
#include "SagazTheme.h"

namespace
{
    namespace Colours = SagazTheme::Colours;
    namespace Metrics = SagazTheme::Metrics;

    const juce::Identifier buttonStyleProperty { "sagazButtonStyle" };
    const juce::Identifier textStyleProperty { "sagazTextStyle" };

    SagazLookAndFeel::ButtonStyle getButtonStyle (const juce::Button& button)
    {
        const auto& value = button.getProperties()[buttonStyleProperty];
        return value.isVoid() ? SagazLookAndFeel::ButtonStyle::action : static_cast<SagazLookAndFeel::ButtonStyle> ((int) value);
    }

    // A point at an angle measured clockwise from 12 o'clock, as JUCE's rotary angles are
    juce::Point<float> pointOnCircle (juce::Point<float> centre, float radius, float angle)
    {
        return { centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle) };
    }

    void strokeArc (juce::Graphics& g, juce::Point<float> centre, float radius, float fromAngle, float toAngle, float width)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, fromAngle, toAngle, true);
        g.strokePath (arc, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Accent shade for a control's state
    juce::Colour accentFor (bool isEnabled, bool isHighlighted, bool isDown)
    {
        if (!isEnabled)
            return Colours::textDisabled;
        return isDown ? Colours::accentPressed : (isHighlighted ? Colours::accentHover : Colours::accent);
    }
}

SagazLookAndFeel::SagazLookAndFeel()
{
    setUpColours();
}

void SagazLookAndFeel::setUpColours()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::window);
    setColour (juce::Label::textColourId, Colours::textPrimary);
    setColour (juce::Slider::textBoxTextColourId, Colours::textPrimary);
    setColour (juce::Slider::textBoxBackgroundColourId, Colours::inset);
    setColour (juce::Slider::textBoxOutlineColourId, Colours::accent); // shown while typing a value
    setColour (juce::Slider::textBoxHighlightColourId, Colours::accentFill);
    setColour (juce::TextEditor::backgroundColourId, Colours::inset);
    setColour (juce::TextEditor::textColourId, Colours::textPrimary);
    setColour (juce::TextEditor::highlightColourId, Colours::accentFill);
    setColour (juce::TextEditor::outlineColourId, Colours::border);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::accent);
    setColour (juce::CaretComponent::caretColourId, Colours::accent);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::surface);
    setColour (juce::TooltipWindow::textColourId, Colours::textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, Colours::borderStrong);
}

void SagazLookAndFeel::setButtonStyle (juce::Button& button, ButtonStyle style)
{
    button.getProperties().set (buttonStyleProperty, static_cast<int> (style));
}

void SagazLookAndFeel::setTextStyle (juce::Label& label, SagazTextStyle style)
{
    label.getProperties().set (textStyleProperty, static_cast<int> (style));
}

//==============================================================================
// Any text drawn with the default font (tooltips, text editors) uses Inter too
juce::Typeface::Ptr SagazLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return font.isBold() ? fonts->getBold() : fonts->getMedium();

    return LookAndFeel_V4::getTypefaceForFont (font);
}

juce::Font SagazLookAndFeel::getLabelFont (juce::Label& label)
{
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
        return fonts->get (SagazTextStyle::value);

    const auto& value = label.getProperties()[textStyleProperty];
    return fonts->get (value.isVoid() ? SagazTextStyle::label : static_cast<SagazTextStyle> ((int) value));
}

juce::Font SagazLookAndFeel::getTextButtonFont (juce::TextButton& button, int)
{
    return fonts->get (getButtonStyle (button) == ButtonStyle::tab ? SagazTextStyle::tab : SagazTextStyle::button);
}

//==============================================================================
void SagazLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent()))
    {
        drawSliderValue (g, label, *slider);
        return;
    }

    if (label.isBeingEdited())
        return;

    g.setColour (label.isEnabled() ? label.findColour (juce::Label::textColourId) : Colours::textDisabled);
    g.setFont (getLabelFont (label));
    g.drawFittedText (label.getText(), label.getLocalBounds(), label.getJustificationType(), 1, 1.0f);
}

// A knob's value box (inset, outlined in accent while dragging), or a slider bar's printed value
void SagazLookAndFeel::drawSliderValue (juce::Graphics& g, juce::Label& label, juce::Slider& slider)
{
    const auto enabled = slider.isEnabled();

    if (slider.getSliderStyle() != juce::Slider::LinearBar)
    {
        const auto box = label.getLocalBounds().toFloat().reduced (0.5f);
        if (enabled)
        {
            g.setColour (Colours::inset);
            g.fillRoundedRectangle (box, SagazTheme::Radius::small);
        }
        g.setColour (slider.isMouseButtonDown() ? Colours::accent : Colours::border);
        g.drawRoundedRectangle (box, SagazTheme::Radius::small, SagazTheme::Stroke::control);
    }

    if (label.isBeingEdited())
        return;

    g.setColour (enabled ? Colours::textPrimary : Colours::textDisabled);
    g.setFont (fonts->get (SagazTextStyle::value));
    g.drawText (label.getText(), label.getLocalBounds(), juce::Justification::centred, false);
}

//==============================================================================
void SagazLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool isHighlighted, bool isDown)
{
    switch (getButtonStyle (button))
    {
        case ButtonStyle::tab: drawTab (g, button, isHighlighted, isDown); break;
        case ButtonStyle::segment: drawSegment (g, button, isHighlighted, isDown); break;
        case ButtonStyle::action: drawActionButton (g, button, isHighlighted, isDown); break;
    }
}

// Momentary: fills with accent only while held
void SagazLookAndFeel::drawActionButton (juce::Graphics& g, juce::Button& button, bool isHighlighted, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto enabled = button.isEnabled();

    if (enabled)
    {
        g.setColour (isDown ? Colours::accent : (isHighlighted ? Colours::surfaceHover : Colours::surface));
        g.fillRoundedRectangle (bounds, SagazTheme::Radius::small);
    }

    g.setColour (!enabled ? Colours::border : (isDown ? Colours::accent : Colours::borderStrong));
    g.drawRoundedRectangle (bounds, SagazTheme::Radius::small, SagazTheme::Stroke::control);
}

// Selected tab is raised and underlined in accent; the tab strip's frame is drawn by its owner
void SagazLookAndFeel::drawTab (juce::Graphics& g, juce::Button& button, bool isHighlighted, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto selected = button.getToggleState();
    const auto fill = selected ? Colours::surface : (isDown ? Colours::surfacePressed : (isHighlighted ? Colours::surfaceHover : juce::Colours::transparentBlack));

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, SagazTheme::Radius::medium - 2.0f);

    if (selected)
    {
        g.setColour (isDown ? Colours::accentPressed : Colours::accent);
        g.fillRoundedRectangle (bounds.withTrimmedLeft (12.0f).withTrimmedRight (12.0f).removeFromBottom (2.0f), 1.0f);
    }
}

// One cell of a segmented group: only the group's outer corners are rounded; the group's
// outline is drawn by its owner
void SagazLookAndFeel::drawSegment (juce::Graphics& g, juce::Button& button, bool isHighlighted, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto enabled = button.isEnabled();
    const auto fill = button.getToggleState() ? (enabled ? accentFor (true, isHighlighted, isDown) : Colours::surfacePressed)
                                              : (isDown ? Colours::surfacePressed : (isHighlighted ? Colours::surfaceHover : Colours::surface));

    const auto left = !button.isConnectedOnLeft();
    const auto right = !button.isConnectedOnRight();
    juce::Path cell;
    cell.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), SagazTheme::Radius::medium - 1.0f, SagazTheme::Radius::medium - 1.0f, left, right, left, right);
    g.setColour (fill);
    g.fillPath (cell);

    if (button.isConnectedOnLeft())
    {
        g.setColour (Colours::border);
        g.fillRect (bounds.withWidth (SagazTheme::Stroke::hair));
    }
}

void SagazLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool isHighlighted, bool isDown)
{
    const auto enabled = button.isEnabled();
    const auto selected = button.getToggleState();
    auto colour = Colours::textPrimary;

    switch (getButtonStyle (button))
    {
        case ButtonStyle::tab: colour = selected || isHighlighted ? Colours::textPrimary : Colours::textSecondary; break;
        case ButtonStyle::segment: colour = !enabled ? Colours::textDisabled : (selected ? Colours::textOnAccent : Colours::textPrimary); break;
        case ButtonStyle::action: colour = !enabled ? Colours::textDisabled : (isDown ? Colours::textOnAccent : Colours::textPrimary); break;
    }

    g.setColour (colour);
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

//==============================================================================
// Checkbox at the left, label after it; the whole row is the hit area
void SagazLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isHighlighted, bool isDown)
{
    const auto boxSize = (float) Metrics::switchBox;
    const auto box = juce::Rectangle<float> (0.0f, ((float) button.getHeight() - boxSize) * 0.5f, boxSize, boxSize);
    drawSwitchBox (g, box, button, isHighlighted, isDown);

    const auto textLeft = Metrics::switchBox + Metrics::switchLabelGap;
    g.setColour (button.isEnabled() ? Colours::textPrimary : Colours::textDisabled);
    g.setFont (fonts->get (SagazTextStyle::switchLabel));
    g.drawText (button.getButtonText(), button.getLocalBounds().withTrimmedLeft (textLeft), juce::Justification::centredLeft, false);
}

void SagazLookAndFeel::drawSwitchBox (juce::Graphics& g, juce::Rectangle<float> box, juce::ToggleButton& button, bool isHighlighted, bool isDown)
{
    const auto enabled = button.isEnabled();
    const auto on = button.getToggleState();
    const auto fill = on ? accentFor (enabled, isHighlighted, isDown) : (isDown ? Colours::surfacePressed : (isHighlighted ? Colours::surfaceHover : Colours::inset));
    const auto outline = on ? fill : (!enabled ? Colours::border : (isHighlighted ? Colours::textSecondary : Colours::borderStrong));

    const auto frame = box.reduced (SagazTheme::Stroke::control * 0.5f);
    g.setColour (fill);
    g.fillRoundedRectangle (frame, SagazTheme::Radius::small);
    g.setColour (outline);
    g.drawRoundedRectangle (frame, SagazTheme::Radius::small, SagazTheme::Stroke::control);

    if (!on)
        return;

    juce::Path tick;
    tick.startNewSubPath (box.getX() + 3.6f, box.getY() + 7.2f);
    tick.lineTo (box.getX() + 6.0f, box.getY() + 9.6f);
    tick.lineTo (box.getX() + 10.5f, box.getY() + 4.6f);
    g.setColour (enabled ? Colours::textOnAccent : Colours::panel);
    g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
// Track arc, value arc from the start, cap with outline, pointer line
void SagazLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight(), (float) Metrics::knob);
    const auto centre = bounds.getCentre();
    const auto enabled = slider.isEnabled();
    const auto isDown = slider.isMouseButtonDown();
    const auto isHighlighted = slider.isMouseOverOrDragging();

    const auto arcWidth = SagazTheme::Stroke::arc * (size < 44.0f ? 0.85f : 1.0f);
    const auto arcRadius = size * 0.5f - arcWidth * 0.5f - 1.0f;
    const auto bodyRadius = arcRadius - arcWidth * 0.5f - 3.5f;
    const auto angle = startAngle + position * (endAngle - startAngle);

    g.setColour (enabled ? Colours::knobTrack : Colours::gridBeat);
    strokeArc (g, centre, arcRadius, startAngle, endAngle, arcWidth);

    if (angle - startAngle > 0.01f)
    {
        g.setColour (accentFor (enabled, isHighlighted, isDown));
        strokeArc (g, centre, arcRadius, startAngle, angle, arcWidth);
    }

    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setColour (!enabled ? Colours::knobBody : (isDown ? Colours::knobBodyPressed : (isHighlighted ? Colours::knobBodyHover : Colours::knobBody)));
    g.fillEllipse (body);
    g.setColour (enabled ? Colours::knobStroke : Colours::border);
    g.drawEllipse (body, SagazTheme::Stroke::control);

    g.setColour (enabled ? Colours::textPrimary : Colours::textDisabled);
    const juce::Line<float> pointer (pointOnCircle (centre, bodyRadius * 0.3f, angle), pointOnCircle (centre, bodyRadius - 2.0f, angle));
    g.drawLine (pointer, SagazTheme::Stroke::pointer);
}

void SagazLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float position, float minPosition, float maxPosition, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style == juce::Slider::LinearBar)
        drawSliderBar (g, position, slider);
    else
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, position, minPosition, maxPosition, style, slider);
}

// Inset bar filled up to the value, the fill's edge a 2 px accent line
void SagazLookAndFeel::drawSliderBar (juce::Graphics& g, float position, juce::Slider& slider)
{
    const auto frame = slider.getLocalBounds().toFloat().reduced (0.5f);
    const auto enabled = slider.isEnabled();
    const auto isDown = slider.isMouseButtonDown();

    g.setColour (Colours::inset);
    g.fillRoundedRectangle (frame, SagazTheme::Radius::small);

    {
        juce::Graphics::ScopedSaveState clip (g);
        juce::Path rounded;
        rounded.addRoundedRectangle (frame, SagazTheme::Radius::small);
        g.reduceClipRegion (rounded);

        const auto fill = frame.withRight (juce::jlimit (frame.getX(), frame.getRight(), position));
        g.setColour (enabled ? Colours::accentFill : Colours::gridBeat);
        g.fillRect (fill);
        g.setColour (!enabled ? Colours::textDisabled : (isDown ? Colours::accentHover : Colours::accent));
        g.fillRect (fill.withLeft (fill.getRight() - 2.0f));
    }

    const auto outline = !enabled ? Colours::border : (isDown ? Colours::accent : (slider.isMouseOver (true) ? Colours::textSecondary : Colours::borderStrong));
    g.setColour (outline);
    g.drawRoundedRectangle (frame, SagazTheme::Radius::small, SagazTheme::Stroke::control);
}

// Knobs: the value box sits a gap below the knob, at a fixed size
juce::Slider::SliderLayout SagazLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    if (!slider.isRotary() || slider.getTextBoxPosition() != juce::Slider::TextBoxBelow)
        return LookAndFeel_V4::getSliderLayout (slider);

    auto bounds = slider.getLocalBounds();
    juce::Slider::SliderLayout layout;
    layout.textBoxBounds = bounds.removeFromBottom (Metrics::valueBoxHeight).withSizeKeepingCentre (Metrics::valueBoxWidth, Metrics::valueBoxHeight);
    bounds.removeFromBottom (Metrics::stackGap);
    layout.sliderBounds = bounds;
    return layout;
}
