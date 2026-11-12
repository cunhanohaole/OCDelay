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
      apvts (*this, nullptr, stateTag, createParameterLayout()),
      curveTables (makeCurveTable (curve))
{
    delayTimeMs = apvts.getRawParameterValue (ParamIDs::delayTime.getParamID());
    mixPercent = apvts.getRawParameterValue (ParamIDs::mix.getParamID());
    pingPong = apvts.getRawParameterValue (ParamIDs::pingPong.getParamID());
    sync = apvts.getRawParameterValue (ParamIDs::sync.getParamID());
    division = apvts.getRawParameterValue (ParamIDs::division.getParamID());
    length = apvts.getRawParameterValue (ParamIDs::length.getParamID());
    threshold = apvts.getRawParameterValue (ParamIDs::threshold.getParamID());

    jassert (delayTimeMs != nullptr && mixPercent != nullptr
             && pingPong != nullptr && sync != nullptr && division != nullptr
             && length != nullptr && threshold != nullptr);
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

// The repeats stop exactly at the end of Length
double PluginProcessor::getTailLengthSeconds() const
{
    return computeLengthSeconds (uiBpm.load(), uiBeatsPerBar.load());
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
    currentSampleRate = sampleRate;
    repeaterEngine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels(), kMaxSliceSeconds);
    pushParametersToEngine();
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
    updateHostTiming();
    pushParametersToEngine();
    repeaterEngine.process (buffer, curveTables.read());
    uiPlayhead.store (repeaterEngine.getPlayheadPosition());
}

void PluginProcessor::pushParametersToEngine()
{
    const auto sliceMs = computeSliceMs();
    effectiveSliceMs.store (sliceMs);

    repeaterEngine.setSliceSamples (juce::roundToInt (sliceMs * currentSampleRate / 1000.0));
    repeaterEngine.setLengthSamples (juce::roundToInt (computeLengthSeconds (hostBpm, hostBeatsPerBar) * currentSampleRate));
    repeaterEngine.setThresholdDb (threshold->load());
    repeaterEngine.setMix (mixPercent->load() / 100.0f);
    repeaterEngine.setPingPong (pingPong->load() >= 0.5f);
}

// Sync off: the Slice knob in ms. Sync on: the note division at the host tempo.
float PluginProcessor::computeSliceMs()
{
    if (sync->load() < 0.5f)
        return delayTimeMs->load();

    return (float) TempoSync::divisionToMs (juce::roundToInt (division->load()), hostBpm);
}

void PluginProcessor::updateHostTiming()
{
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 0.0)
                hostBpm = *bpm;

            if (const auto signature = position->getTimeSignature(); signature.hasValue() && signature->numerator > 0 && signature->denominator > 0)
                hostBeatsPerBar = signature->numerator * 4.0 / signature->denominator;
        }
    }

    uiBpm.store ((float) hostBpm);
    uiBeatsPerBar.store ((float) hostBeatsPerBar);
}

double PluginProcessor::computeLengthSeconds (double bpm, double beatsPerBar) const
{
    return getLengthBars() * beatsPerBar * 60.0 / juce::jmax (1.0, bpm);
}

int PluginProcessor::getLengthBars() const
{
    const auto index = juce::jlimit (0, (int) ParamRanges::lengthBarsOptions.size() - 1, juce::roundToInt (length->load()));
    return ParamRanges::lengthBarsOptions[(size_t) index];
}

void PluginProcessor::updateCurve (const VolumeCurve& newCurve)
{
    curve = newCurve;
    curve.fillTable (curveTables.getWriteBuffer());
    curveTables.publish();
    ++curveVersion;
}

PluginProcessor::CurveTable PluginProcessor::makeCurveTable (const VolumeCurve& source)
{
    CurveTable table {};
    source.fillTable (table);
    return table;
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
// Saved state = APVTS parameters plus the volume curve as a child tree
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName (VolumeCurve::treeType), nullptr);
    state.appendChild (curve.toValueTree(), nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

// Hosts call this on the message thread, which is the only thread that edits the curve.
// v0.1 sessions have no curve child and get the default curve.
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || !xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const auto curveTree = state.getChildWithName (VolumeCurve::treeType);
    state.removeChild (curveTree, nullptr);

    apvts.replaceState (state);
    updateCurve (VolumeCurve::fromValueTree (curveTree));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
