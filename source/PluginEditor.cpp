#include "PluginEditor.h"
#include "Parameters.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      delayTimeKnob (p.getAPVTS(), ParamIDs::delayTime, "Delay Time"),
      feedbackKnob (p.getAPVTS(), ParamIDs::feedback, "Feedback"),
      mixKnob (p.getAPVTS(), ParamIDs::mix, "Mix"),
      pingPongAttachment (p.getAPVTS(), ParamIDs::pingPong.getParamID(), pingPongButton)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (delayTimeKnob);
    addAndMakeVisible (feedbackKnob);
    addAndMakeVisible (mixKnob);
    addAndMakeVisible (pingPongButton);

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
    const auto strip = area.removeFromBottom (proportionOfHeight (0.4f));

    // The area above the strip is reserved for the future curve editor
    layoutControlStrip (strip.reduced (proportionOfWidth (0.02f), proportionOfHeight (0.02f)));

#if JUCE_DEBUG
    inspectButton.setBounds (area.removeFromTop (proportionOfHeight (0.08f))
                                 .removeFromRight (proportionOfWidth (0.14f))
                                 .reduced (proportionOfWidth (0.01f)));
#endif
}

// Four equal columns: Delay Time, Feedback, Mix, Ping-Pong
void PluginEditor::layoutControlStrip (juce::Rectangle<int> strip)
{
    const auto columnWidth = strip.getWidth() / 4;

    delayTimeKnob.setBounds (strip.removeFromLeft (columnWidth));
    feedbackKnob.setBounds (strip.removeFromLeft (columnWidth));
    mixKnob.setBounds (strip.removeFromLeft (columnWidth));

    const auto toggleWidth = juce::jmin (strip.getWidth(), proportionOfWidth (0.18f));
    pingPongButton.setBounds (strip.withSizeKeepingCentre (toggleWidth, proportionOfHeight (0.08f)));
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
