#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    constexpr float kControlStripHeight = 0.3f; // fraction of the window height
    constexpr float kSwitchHeight = 0.07f;
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      delayTimeKnob (p.getAPVTS(), ParamIDs::delayTime, "Delay Time"),
      divisionKnob (p.getAPVTS(), ParamIDs::division, "Delay Time"),
      feedbackKnob (p.getAPVTS(), ParamIDs::feedback, "Feedback"),
      mixKnob (p.getAPVTS(), ParamIDs::mix, "Mix"),
      syncAttachment (p.getAPVTS(), ParamIDs::sync.getParamID(), syncButton),
      pingPongAttachment (p.getAPVTS(), ParamIDs::pingPong.getParamID(), pingPongButton),
      syncVisibilityAttachment (*p.getAPVTS().getParameter (ParamIDs::sync.getParamID()),
          [this] (float value) { showDivisionKnob (value >= 0.5f); })
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (delayTimeKnob);
    addChildComponent (divisionKnob);
    addAndMakeVisible (feedbackKnob);
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

    // The area above the strip is reserved for the future curve editor
    layoutControlStrip (strip.reduced (proportionOfWidth (0.02f), proportionOfHeight (0.02f)));

#if JUCE_DEBUG
    inspectButton.setBounds (area.removeFromTop (proportionOfHeight (0.08f))
                                 .removeFromRight (proportionOfWidth (0.14f))
                                 .reduced (proportionOfWidth (0.01f)));
#endif
}

// Four equal columns: Delay Time / Division, Feedback, Mix, switches
void PluginEditor::layoutControlStrip (juce::Rectangle<int> strip)
{
    const auto columnWidth = strip.getWidth() / 4;

    const auto delayColumn = strip.removeFromLeft (columnWidth);
    delayTimeKnob.setBounds (delayColumn);
    divisionKnob.setBounds (delayColumn);

    feedbackKnob.setBounds (strip.removeFromLeft (columnWidth));
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
