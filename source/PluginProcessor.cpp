#include "PluginProcessor.h"
#include "Parameters.h"
#include "TempoSync.h"
#include "dsp/SpeedMapping.h"
#include "PluginEditor.h"

namespace
{
    const juce::Identifier stateTag { "OCDelayState" };

    // The volume curve is saved as a bare "Curve" child (v0.2); the speed curve is wrapped
    // in this child so the two can be told apart
    const juce::Identifier speedCurveTag { "SpeedCurve" };

    // v0.3-v0.4 saved a pitch curve here. It's dropped on load: speed curves mean something else.
    const juce::Identifier retiredPitchCurveTag { "PitchCurve" };

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
    mixPercent = apvts.getRawParameterValue (ParamIDs::mix.getParamID());
    pingPong = apvts.getRawParameterValue (ParamIDs::pingPong.getParamID());
    sync = apvts.getRawParameterValue (ParamIDs::sync.getParamID());
    division = apvts.getRawParameterValue (ParamIDs::division.getParamID());
    length = apvts.getRawParameterValue (ParamIDs::length.getParamID());
    threshold = apvts.getRawParameterValue (ParamIDs::threshold.getParamID());
    pingPongWidth = apvts.getRawParameterValue (ParamIDs::pingPongWidth.getParamID());
    freeRate = apvts.getRawParameterValue (ParamIDs::freeRate.getParamID());
    rateMs = apvts.getRawParameterValue (ParamIDs::rate.getParamID());
    rateDivision = apvts.getRawParameterValue (ParamIDs::rateDivision.getParamID());
    rateOverlap = apvts.getRawParameterValue (ParamIDs::rateOverlap.getParamID());
    endRateMs = apvts.getRawParameterValue (ParamIDs::endRate.getParamID());
    endRateDivision = apvts.getRawParameterValue (ParamIDs::endRateDivision.getParamID());
    maxSpeed = apvts.getRawParameterValue (ParamIDs::maxSpeed.getParamID());
    pitchFollow = apvts.getRawParameterValue (ParamIDs::pitchFollow.getParamID());
    stopHeld = apvts.getRawParameterValue (ParamIDs::stop.getParamID());
    boostPercent = apvts.getRawParameterValue (ParamIDs::boost.getParamID());

    jassert (delayTimeMs != nullptr && mixPercent != nullptr
             && pingPong != nullptr && sync != nullptr && division != nullptr
             && length != nullptr && threshold != nullptr && pingPongWidth != nullptr
             && freeRate != nullptr && rateMs != nullptr && rateDivision != nullptr && rateOverlap != nullptr
             && endRateMs != nullptr && endRateDivision != nullptr && maxSpeed != nullptr && pitchFollow != nullptr && stopHeld != nullptr
             && boostPercent != nullptr);
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
    stopOnPress();
    repeaterEngine.process (buffer, volumeCurve.read(), speedCurve.read());
    uiPlayhead.store (repeaterEngine.getPlayheadPosition());
}

void PluginProcessor::pushParametersToEngine()
{
    const auto sliceMs = computeSliceMs();
    effectiveSliceMs.store (sliceMs);

    repeaterEngine.setSliceSamples (juce::roundToInt (sliceMs * currentSampleRate / 1000.0));

    // Free Rate off: 0 tells the engine to repeat back to back, every slice
    const auto intervalMs = isFreeRateOn() ? computeRateMs() : sliceMs;
    effectiveIntervalMs.store (intervalMs);
    repeaterEngine.setIntervalSamples (isFreeRateOn() ? juce::roundToInt (intervalMs * currentSampleRate / 1000.0) : 0);
    repeaterEngine.setOverlap (rateOverlap->load() >= 0.5f);
    repeaterEngine.setLengthSamples (juce::roundToInt (computeLengthSeconds (hostBpm, hostBeatsPerBar) * currentSampleRate));
    repeaterEngine.setThresholdDb (threshold->load());
    repeaterEngine.setMix (mixPercent->load() / 100.0f);
    repeaterEngine.setPingPong (pingPong->load() >= 0.5f);
    repeaterEngine.setPingPongWidth (pingPongWidth->load() / 100.0f);

    const auto endIntervalMs = computeEndRateMs();
    effectiveEndIntervalMs.store (endIntervalMs);
    repeaterEngine.setEndIntervalSamples (endIntervalMs * currentSampleRate / 1000.0);
    repeaterEngine.setPitchFollow (pitchFollow->load() >= 0.5f);
    repeaterEngine.setBoost (boostPercent->load() / 100.0f);
}

// Stop is momentary: only the press stops the repeats, so holding it doesn't keep silencing new hits
void PluginProcessor::stopOnPress()
{
    const auto held = stopHeld->load() >= 0.5f;
    if (held && ! stopWasHeld)
        repeaterEngine.stop();

    stopWasHeld = held;
}

// Sync off: the Slice knob in ms. Sync on: the note division at the host tempo.
float PluginProcessor::computeSliceMs()
{
    if (sync->load() < 0.5f)
        return delayTimeMs->load();

    return (float) TempoSync::divisionToMs (juce::roundToInt (division->load()), hostBpm);
}

// Same rule as Slice: ms with Sync off, the Rate Division at the host tempo with Sync on
float PluginProcessor::computeRateMs()
{
    if (sync->load() < 0.5f)
        return rateMs->load();

    return (float) TempoSync::divisionToMs (juce::roundToInt (rateDivision->load()), hostBpm);
}

// Top of the speed curve: Max, or End Rate following the same Sync rule as Slice and Rate
float PluginProcessor::computeEndRateMs()
{
    if (maxSpeed->load() >= 0.5f)
        return (float) SpeedMapping::kMaxSpeedIntervalMs;

    if (sync->load() < 0.5f)
        return endRateMs->load();

    return (float) TempoSync::divisionToMs (juce::roundToInt (endRateDivision->load()), hostBpm);
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

            stopWhenTransportStops (position->getIsPlaying());
        }
    }

    uiBpm.store ((float) hostBpm);
    uiBeatsPerBar.store ((float) hostBeatsPerBar);
}

// Stopping the host's transport kills the repeats, like pressing Stop. Only the moment it stops
// counts: hosts keep processing while stopped, so hits played live still repeat.
void PluginProcessor::stopWhenTransportStops (bool hostIsPlaying)
{
    if (hostWasPlaying && ! hostIsPlaying)
        repeaterEngine.stop();

    hostWasPlaying = hostIsPlaying;
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

const EnvelopeCurve& PluginProcessor::getCurve (CurveTarget target) const
{
    return target == CurveTarget::speed ? speedCurve.getCurve() : volumeCurve.getCurve();
}

void PluginProcessor::updateCurve (const EnvelopeCurve& newCurve, CurveTarget target)
{
    getSlot (target).update (newCurve);
    ++curveVersion;
}

CurveSlot& PluginProcessor::getSlot (CurveTarget target)
{
    return target == CurveTarget::speed ? speedCurve : volumeCurve;
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
// Saved state = APVTS parameters plus both curves as child trees
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    appendCurveTrees (state);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

// Hosts call this on the message thread, which is the only thread that edits the curves.
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || !xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    restoreCurves (state);
    apvts.replaceState (state);
}

void PluginProcessor::appendCurveTrees (juce::ValueTree& state) const
{
    state.removeChild (state.getChildWithName (EnvelopeCurve::treeType), nullptr);
    state.removeChild (state.getChildWithName (speedCurveTag), nullptr);

    juce::ValueTree speedTree { speedCurveTag };
    speedTree.appendChild (speedCurve.getCurve().toValueTree(), nullptr);

    state.appendChild (volumeCurve.getCurve().toValueTree(), nullptr);
    state.appendChild (speedTree, nullptr);
}

// Takes the curve children out of state so APVTS only sees parameters.
// Older sessions without a curve get the defaults.
void PluginProcessor::restoreCurves (juce::ValueTree& state)
{
    const auto volumeTree = state.getChildWithName (EnvelopeCurve::treeType);
    const auto speedTree = state.getChildWithName (speedCurveTag);
    state.removeChild (volumeTree, nullptr);
    state.removeChild (speedTree, nullptr);
    state.removeChild (state.getChildWithName (retiredPitchCurveTag), nullptr);

    updateCurve (EnvelopeCurve::fromValueTree (volumeTree, defaultVolumeCurve()), CurveTarget::volume);
    updateCurve (EnvelopeCurve::fromValueTree (speedTree.getChildWithName (EnvelopeCurve::treeType), defaultSpeedCurve()), CurveTarget::speed);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
