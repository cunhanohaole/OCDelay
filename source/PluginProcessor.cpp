#include "PluginProcessor.h"
#include "Parameters.h"
#include "TempoSync.h"
#include "PluginEditor.h"

namespace
{
    const juce::Identifier stateTag { "OCDelayState" };

    bool isMonoOrStereo (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    }
}

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, stateTag, createParameterLayout())
{
    delayTimeMs = apvts.getRawParameterValue (ParamIDs::delayTime.getParamID());
    feedbackPercent = apvts.getRawParameterValue (ParamIDs::feedback.getParamID());
    mixPercent = apvts.getRawParameterValue (ParamIDs::mix.getParamID());
    pingPong = apvts.getRawParameterValue (ParamIDs::pingPong.getParamID());
    sync = apvts.getRawParameterValue (ParamIDs::sync.getParamID());
    division = apvts.getRawParameterValue (ParamIDs::division.getParamID());

    jassert (delayTimeMs != nullptr && feedbackPercent != nullptr && mixPercent != nullptr
             && pingPong != nullptr && sync != nullptr && division != nullptr);
}

PluginProcessor::~PluginProcessor() = default;

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
    return false;
}

bool PluginProcessor::producesMidi() const
{
    return false;
}

bool PluginProcessor::isMidiEffect() const
{
    return false;
}

// Time until the repeats have decayed by 60 dB at the current settings
double PluginProcessor::getTailLengthSeconds() const
{
    const auto delaySeconds = juce::jmin (kMaxDelaySeconds, (double) effectiveDelayMs.load() / 1000.0);
    const auto feedbackGain = juce::jlimit (0.0, (double) DelayEngine::kMaxFeedback, (double) feedbackPercent->load() / 100.0);

    if (feedbackGain <= 0.0)
        return delaySeconds;

    const auto repeatsUntilSilent = std::ceil (std::log (0.001) / std::log (feedbackGain));
    return delaySeconds * (1.0 + repeatsUntilSilent);
}

int PluginProcessor::getNumPrograms()
{
    return 1; // Some hosts don't cope with 0 programs
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    // Steinberg's VST3 validator fails plugins whose single default program has no name
    return "Default";
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    pushParametersToEngine();
    delayEngine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels(), kMaxDelaySeconds);
}

void PluginProcessor::releaseResources()
{
}

// Supported: mono -> mono, mono -> stereo, stereo -> stereo
bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (!isMonoOrStereo (input) || !isMonoOrStereo (output))
        return false;

    return input.size() <= output.size();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Output channels without a matching input may contain garbage
    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    copyMonoInputToStereo (buffer);
    pushParametersToEngine();
    delayEngine.process (buffer);
}

void PluginProcessor::pushParametersToEngine()
{
    const auto delayMs = computeDelayTimeMs();
    effectiveDelayMs.store (delayMs);
    delayEngine.setDelayTimeMs (delayMs);
    delayEngine.setFeedback (feedbackPercent->load() / 100.0f);
    delayEngine.setMix (mixPercent->load() / 100.0f);
    delayEngine.setPingPong (pingPong->load() >= 0.5f);
}

// Sync off: the Delay Time knob in ms. Sync on: the note division at the host tempo.
float PluginProcessor::computeDelayTimeMs()
{
    if (sync->load() < 0.5f)
        return delayTimeMs->load();

    updateHostBpm();
    return (float) TempoSync::divisionToMs (juce::roundToInt (division->load()), hostBpm);
}

void PluginProcessor::updateHostBpm()
{
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            if (const auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 0.0)
                hostBpm = *bpm;
}

// Mono in, stereo out: both sides start from the same signal, so the plain delay is
// centred and ping-pong (which sums to mono) bounces the mono input
void PluginProcessor::copyMonoInputToStereo (juce::AudioBuffer<float>& buffer) const
{
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
