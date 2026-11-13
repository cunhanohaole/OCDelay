#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    constexpr float kControlStripHeight = 0.3f; // fraction of the window height
    constexpr float kSwitchHeight = 0.058f;
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
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    const auto strip = area.removeFromBottom (proportionOfHeight (kControlStripHeight));

    layoutControlStrip (strip.reduced (proportionOfWidth (0.02f), proportionOfHeight (0.02f)));
    curveEditor.setBounds (area.reduced (proportionOfWidth (0.02f), proportionOfHeight (0.02f)));

#if JUCE_DEBUG
    // In the curve editor's top bar, just right of the Volume / Speed tabs
    inspectButton.setBounds (curveEditor.getBounds()
                                 .removeFromTop (curveEditor.proportionOfHeight (0.12f))
                                 .withTrimmedLeft (curveEditor.proportionOfWidth (CurveToolbar::kTabsWidth + 0.01f))
                                 .withWidth (proportionOfWidth (0.1f))
                                 .reduced (2));
#endif
}

// Slice / Division, Rate / Rate Division, Length (wider, for six buttons), Threshold, Mix,
// Width, switches
void PluginEditor::layoutControlStrip (juce::Rectangle<int> strip)
{
    const auto columnWidth = juce::roundToInt ((float) strip.getWidth() / 8.2f);

    const auto sliceColumn = strip.removeFromLeft (columnWidth);
    delayTimeKnob.setBounds (sliceColumn);
    divisionKnob.setBounds (sliceColumn);

    const auto rateColumn = strip.removeFromLeft (columnWidth);
    rateKnob.setBounds (rateColumn);
    rateDivisionKnob.setBounds (rateColumn);

    layoutLengthColumn (strip.removeFromLeft (juce::roundToInt ((float) columnWidth * 1.8f)).reduced (proportionOfWidth (0.01f), 0));
    thresholdKnob.setBounds (strip.removeFromLeft (columnWidth));
    mixKnob.setBounds (strip.removeFromLeft (columnWidth));
    widthKnob.setBounds (strip.removeFromLeft (columnWidth));

    layoutSwitches (strip);
}

// Length keeps the whole column so its buttons line up with the knobs; Stop sits in the empty
// band below them, level with the knobs' value boxes (and above the selector, so it gets the clicks)
void PluginEditor::layoutLengthColumn (juce::Rectangle<int> column)
{
    lengthSelector.setBounds (column);

    const auto stopRow = column.withTrimmedTop (column.proportionOfHeight (0.8f)).withTrimmedBottom (column.proportionOfHeight (0.03f));
    stopButton.setBounds (stopRow.withSizeKeepingCentre (column.proportionOfWidth (0.36f), stopRow.getHeight()));
}

// Sync, Free Rate, Overlap, Ping-Pong stacked and centred in the column
void PluginEditor::layoutSwitches (juce::Rectangle<int> column)
{
    const auto switchWidth = juce::jmin (column.getWidth(), proportionOfWidth (0.14f));
    const auto switchHeight = proportionOfHeight (kSwitchHeight);
    auto switches = column.withSizeKeepingCentre (switchWidth, 4 * switchHeight);

    for (auto* button : { &syncButton, &freeRateButton, &overlapButton, &pingPongButton })
        button->setBounds (switches.removeFromTop (switchHeight));
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
