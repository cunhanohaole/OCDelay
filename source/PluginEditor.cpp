#include "PluginEditor.h"
#include "Parameters.h"
#include "ui/sagaz/SagazTheme.h"

namespace
{
    namespace Metrics = SagazTheme::Metrics;

    // Window layout (px)
    const juce::Rectangle<int> kCurvePanel { 10, 10, 840, 312 };
    const juce::Rectangle<int> kControlStrip { 22, 342, 816, 92 };
    constexpr int kLengthColumnWidth = 166;
    constexpr int kSwitchColumnWidth = 96;
    constexpr int kLogoColumnWidth = 66;
    constexpr int kSwitchGap = 6;
    constexpr int kStopTop = 72; // in the Length column, below its buttons
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      curveEditor (p),
      delayTimeKnob (p.getAPVTS(), ParamIDs::delayTime, "Slice"),
      divisionKnob (p.getAPVTS(), ParamIDs::division, "Slice"),
      rateKnob (p.getAPVTS(), ParamIDs::rate, "Rate"),
      rateDivisionKnob (p.getAPVTS(), ParamIDs::rateDivision, "Rate"),
      lengthSelector (*p.getAPVTS().getParameter (ParamIDs::length.getParamID())),
      stopButton (*p.getAPVTS().getParameter (ParamIDs::stop.getParamID())),
      thresholdKnob (p.getAPVTS(), ParamIDs::threshold, "Threshold"),
      mixKnob (p.getAPVTS(), ParamIDs::mix, "Mix"),
      widthKnob (p.getAPVTS(), ParamIDs::pingPongWidth, "Width"),
      syncAttachment (p.getAPVTS(), ParamIDs::sync.getParamID(), syncButton),
      freeRateAttachment (p.getAPVTS(), ParamIDs::freeRate.getParamID(), freeRateButton),
      overlapAttachment (p.getAPVTS(), ParamIDs::rateOverlap.getParamID(), overlapButton),
      pingPongAttachment (p.getAPVTS(), ParamIDs::pingPong.getParamID(), pingPongButton),
      syncVisibilityAttachment (*p.getAPVTS().getParameter (ParamIDs::sync.getParamID()),
          [this] (float value) { showDivisionKnobs (value >= 0.5f); }),
      freeRateEnableAttachment (*p.getAPVTS().getParameter (ParamIDs::freeRate.getParamID()),
          [this] (float value) { enableRateControls (value >= 0.5f); })
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (curveEditor);
    addAndMakeVisible (delayTimeKnob);
    addChildComponent (divisionKnob);
    addAndMakeVisible (rateKnob);
    addChildComponent (rateDivisionKnob);
    addAndMakeVisible (lengthSelector);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (thresholdKnob);
    addAndMakeVisible (mixKnob);
    addAndMakeVisible (widthKnob);
    addAndMakeVisible (syncButton);
    addAndMakeVisible (freeRateButton);
    addAndMakeVisible (overlapButton);
    addAndMakeVisible (pingPongButton);

    syncButton.setTooltip ("Sync Slice and Rate to the host tempo as note divisions");
    freeRateButton.setTooltip ("Start repeats every Rate instead of every Slice");
    overlapButton.setTooltip ("When Rate is shorter than Slice: let repeats ring out and stack instead of cutting each one at the next");
    syncVisibilityAttachment.sendInitialUpdate();
    freeRateEnableAttachment.sendInitialUpdate();

#if JUCE_DEBUG
    setUpInspector();
#endif

    setResizable (false, false);
    setSize (kWidth, kHeight);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (SagazTheme::Colours::window);

    g.setColour (SagazTheme::Colours::border);
    for (const auto& divider : dividers)
        g.fillRect (divider);

    drawLogo (g);
}

// A small accent square, the plugin name and the company, stacked and centred
void PluginEditor::drawLogo (juce::Graphics& g) const
{
    const auto& fonts = lookAndFeel.getFonts();
    auto area = logoArea.withSizeKeepingCentre (logoArea.getWidth(), 44);

    g.setColour (SagazTheme::Colours::accent);
    g.fillRoundedRectangle (area.removeFromTop (8).withWidth (8).toFloat(), SagazTheme::Radius::small);
    area.removeFromTop (4);

    g.setColour (SagazTheme::Colours::textPrimary);
    g.setFont (fonts.get (SagazTextStyle::title));
    g.drawText ("OCDelay", area.removeFromTop (17), juce::Justification::centredLeft, false);
    area.removeFromTop (4);

    g.setColour (SagazTheme::Colours::textSecondary);
    g.setFont (fonts.get (SagazTextStyle::axis).withExtraKerningFactor (0.18f));
    g.drawText ("SAGAZ", area, juce::Justification::centredLeft, false);
}

void PluginEditor::resized()
{
    curveEditor.setBounds (kCurvePanel);
    layoutControlStrip();

#if JUCE_DEBUG
    // At the right of the curve editor's top bar, before Snap
    const auto freeArea = curveEditor.getToolbar().getFreeArea() + curveEditor.getToolbar().getBounds().getPosition() + curveEditor.getPosition();
    inspectButton.setBounds (freeArea.withLeft (freeArea.getRight() - Metrics::buttonMinWidth).withSizeKeepingCentre (Metrics::buttonMinWidth, Metrics::buttonHeight));
#endif
}

// Left to right, spread evenly: Slice / Division, Rate / Rate Division, Length with Stop,
// Threshold, Mix, Width, a divider, the switches, a divider, the plugin name
void PluginEditor::layoutControlStrip()
{
    constexpr int knob = Metrics::knobCellWidth;
    const std::array<int, 10> widths { knob, knob, kLengthColumnWidth, knob, knob, knob, 1, kSwitchColumnWidth, 1, kLogoColumnWidth };

    auto usedWidth = 0;
    for (const auto width : widths)
        usedWidth += width;
    const auto gap = (float) (kControlStrip.getWidth() - usedWidth) / (float) (widths.size() - 1);

    std::array<juce::Rectangle<int>, 10> columns;
    auto x = (float) kControlStrip.getX();
    for (size_t i = 0; i < widths.size(); ++i)
    {
        columns[i] = kControlStrip.withX (juce::roundToInt (x)).withWidth (widths[i]);
        x += (float) widths[i] + gap;
    }

    for (auto* knobPair : { &delayTimeKnob, &divisionKnob })
        knobPair->setBounds (columns[0]);
    for (auto* knobPair : { &rateKnob, &rateDivisionKnob })
        knobPair->setBounds (columns[1]);
    layoutLengthColumn (columns[2]);
    thresholdKnob.setBounds (columns[3]);
    mixKnob.setBounds (columns[4]);
    widthKnob.setBounds (columns[5]);
    dividers = { columns[6], columns[8] };
    layoutSwitches (columns[7]);
    logoArea = columns[9];
}

// Length's buttons line up with the knobs; Stop sits below them, level with the value boxes
// (added after the selector, so it gets the clicks)
void PluginEditor::layoutLengthColumn (juce::Rectangle<int> column)
{
    lengthSelector.setBounds (column);
    stopButton.setBounds (juce::Rectangle<int> (Metrics::buttonMinWidth, Metrics::buttonHeight).withCentre ({ column.getCentreX(), column.getY() + kStopTop + Metrics::buttonHeight / 2 }));
}

// Sync, Free Rate, Overlap, Ping-Pong stacked and centred in the column
void PluginEditor::layoutSwitches (juce::Rectangle<int> column)
{
    constexpr int rowHeight = Metrics::switchBox;
    auto switches = column.withSizeKeepingCentre (column.getWidth(), 4 * rowHeight + 3 * kSwitchGap);

    for (auto* button : { &syncButton, &freeRateButton, &overlapButton, &pingPongButton })
    {
        button->setBounds (switches.removeFromTop (rowHeight));
        switches.removeFromTop (kSwitchGap);
    }
}

void PluginEditor::showDivisionKnobs (bool shouldShowDivisions)
{
    divisionKnob.setVisible (shouldShowDivisions);
    delayTimeKnob.setVisible (!shouldShowDivisions);
    rateDivisionKnob.setVisible (shouldShowDivisions);
    rateKnob.setVisible (!shouldShowDivisions);
}

// Greyed out (and not editable) while repeats follow Slice
void PluginEditor::enableRateControls (bool freeRateOn)
{
    rateKnob.setEnabled (freeRateOn);
    rateDivisionKnob.setEnabled (freeRateOn);
    overlapButton.setEnabled (freeRateOn);
}

#if JUCE_DEBUG
void PluginEditor::setUpInspector()
{
    addAndMakeVisible (inspectButton);

    inspectButton.onClick = [this] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };
}
#endif
