#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    constexpr float kControlStripHeight = 0.3f; // fraction of the window height
    constexpr float kSwitchHeight = 0.07f;
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      curveEditor (p),
      delayTimeKnob (p.getAPVTS(), ParamIDs::delayTime, "Slice"),
      divisionKnob (p.getAPVTS(), ParamIDs::division, "Slice"),
      lengthSelector (*p.getAPVTS().getParameter (ParamIDs::length.getParamID())),
      thresholdKnob (p.getAPVTS(), ParamIDs::threshold, "Threshold"),
      mixKnob (p.getAPVTS(), ParamIDs::mix, "Mix"),
      widthKnob (p.getAPVTS(), ParamIDs::pingPongWidth, "Width"),
      syncAttachment (p.getAPVTS(), ParamIDs::sync.getParamID(), syncButton),
      pingPongAttachment (p.getAPVTS(), ParamIDs::pingPong.getParamID(), pingPongButton),
      syncVisibilityAttachment (*p.getAPVTS().getParameter (ParamIDs::sync.getParamID()),
          [this] (float value) { showDivisionKnob (value >= 0.5f); })
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (curveEditor);
    addAndMakeVisible (delayTimeKnob);
    addChildComponent (divisionKnob);
    addAndMakeVisible (lengthSelector);
    addAndMakeVisible (thresholdKnob);
    addAndMakeVisible (mixKnob);
    addAndMakeVisible (widthKnob);
    addAndMakeVisible (syncButton);
    addAndMakeVisible (pingPongButton);

    syncButton.setTooltip ("Sync the slice length to the host tempo as a note division");
    syncVisibilityAttachment.sendInitialUpdate();

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
    // In the curve editor's top bar, just right of the Volume / Pitch tabs
    inspectButton.setBounds (curveEditor.getBounds()
                                 .removeFromTop (curveEditor.proportionOfHeight (0.12f))
                                 .withTrimmedLeft (curveEditor.proportionOfWidth (CurveToolbar::kTabsWidth + 0.01f))
                                 .withWidth (proportionOfWidth (0.1f))
                                 .reduced (2));
#endif
}

// Slice / Division, Length (wider, for six buttons), Threshold, Mix, Width, switches
void PluginEditor::layoutControlStrip (juce::Rectangle<int> strip)
{
    const auto columnWidth = juce::roundToInt ((float) strip.getWidth() / 6.8f);

    const auto sliceColumn = strip.removeFromLeft (columnWidth);
    delayTimeKnob.setBounds (sliceColumn);
    divisionKnob.setBounds (sliceColumn);

    lengthSelector.setBounds (strip.removeFromLeft (juce::roundToInt ((float) columnWidth * 1.8f)).reduced (proportionOfWidth (0.01f), 0));
    thresholdKnob.setBounds (strip.removeFromLeft (columnWidth));
    mixKnob.setBounds (strip.removeFromLeft (columnWidth));
    widthKnob.setBounds (strip.removeFromLeft (columnWidth));

    layoutSwitches (strip);
}

// Sync above Ping-Pong, centred in the column
void PluginEditor::layoutSwitches (juce::Rectangle<int> column)
{
    const auto switchWidth = juce::jmin (column.getWidth(), proportionOfWidth (0.16f));
    const auto switchHeight = proportionOfHeight (kSwitchHeight);
    auto switches = column.withSizeKeepingCentre (switchWidth, 2 * switchHeight);

    syncButton.setBounds (switches.removeFromTop (switchHeight));
    pingPongButton.setBounds (switches);
}

void PluginEditor::showDivisionKnob (bool shouldShowDivision)
{
    divisionKnob.setVisible (shouldShowDivision);
    delayTimeKnob.setVisible (!shouldShowDivision);
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
