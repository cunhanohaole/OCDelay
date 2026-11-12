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
    addAndMakeVisible (syncButton);
    addAndMakeVisible (pingPongButton);

    syncButton.setTooltip ("Sync the delay time to the host tempo as a note division");
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
    // Top-left corner, in the curve editor's top bar (left of the Snap toggle)
    inspectButton.setBounds (curveEditor.getBounds()
                                 .removeFromTop (curveEditor.proportionOfHeight (0.12f))
                                 .removeFromLeft (proportionOfWidth (0.12f))
                                 .reduced (2));
#endif
}

// Five equal columns: Slice / Division, Length, Threshold, Mix, switches
void PluginEditor::layoutControlStrip (juce::Rectangle<int> strip)
{
    const auto columnWidth = strip.getWidth() / 5;

    const auto sliceColumn = strip.removeFromLeft (columnWidth);
    delayTimeKnob.setBounds (sliceColumn);
    divisionKnob.setBounds (sliceColumn);

    lengthSelector.setBounds (strip.removeFromLeft (columnWidth).reduced (proportionOfWidth (0.01f), 0));
    thresholdKnob.setBounds (strip.removeFromLeft (columnWidth));
    mixKnob.setBounds (strip.removeFromLeft (columnWidth));

    layoutSwitches (strip);
}

// Sync above Ping-Pong, centred in the column
void PluginEditor::layoutSwitches (juce::Rectangle<int> column)
{
    const auto switchWidth = juce::jmin (column.getWidth(), proportionOfWidth (0.18f));
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
