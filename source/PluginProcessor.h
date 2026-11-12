#pragma once

#include "TempoSync.h"
#include "curve/CurveSlot.h"
#include "dsp/RepeaterEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Curve editing: message thread only. updateCurve() publishes a new table to the audio
    // thread lock-free and bumps the version, so an open editor can notice session loads.
    const EnvelopeCurve& getCurve (CurveTarget target = CurveTarget::volume) const;
    void updateCurve (const EnvelopeCurve& newCurve, CurveTarget target = CurveTarget::volume);
    int getCurveVersion() const { return curveVersion.load(); }

    // Pitch curve mapping, for the UI (the parameters' current values)
    float getPitchRangeSemitones() const { return pitchRange->load(); }
    bool isPitchBipolar() const { return pitchBipolar->load() >= 0.5f; }

    // Host timing and playback position for the UI (written by the audio thread)
    float getHostBpm() const { return uiBpm.load(); }
    float getBeatsPerBar() const { return uiBeatsPerBar.load(); }
    float getPlayheadPosition() const { return uiPlayhead.load(); }
    float getSliceMs() const { return effectiveSliceMs.load(); }
    float getRepeatIntervalMs() const { return effectiveIntervalMs.load(); } // Rate, or Slice when Free Rate is off
    int getLengthBars() const;

    // Capture buffers are sized for this. Synced slices can exceed the 2 s ms range:
    // 8 s covers a 1/1 division down to 30 BPM; anything longer is clamped.
    static constexpr double kMaxSliceSeconds = 8.0;

private:
    void pushParametersToEngine();
    float computeSliceMs();
    float computeRateMs();
    bool isFreeRateOn() const { return freeRate->load() >= 0.5f; }
    double computeLengthSeconds (double bpm, double beatsPerBar) const;
    void updateHostTiming();
    static EnvelopeCurve defaultPitchCurve() { return EnvelopeCurve::makeFlat (0.0f); } // no change in unipolar mode
    CurveSlot& getSlot (CurveTarget target);
    void appendCurveTrees (juce::ValueTree& state) const;
    void restoreCurves (juce::ValueTree& state);
    void copyMonoInputToStereo (juce::AudioBuffer<float>& buffer) const;

    juce::AudioProcessorValueTreeState apvts;
    RepeaterEngine repeaterEngine;
    double currentSampleRate = 44100.0;

    // Cached for lock-free reads on the audio thread
    std::atomic<float>* delayTimeMs = nullptr;
    std::atomic<float>* mixPercent = nullptr;
    std::atomic<float>* pingPong = nullptr;
    std::atomic<float>* sync = nullptr;
    std::atomic<float>* division = nullptr;
    std::atomic<float>* length = nullptr;
    std::atomic<float>* threshold = nullptr;
    std::atomic<float>* pingPongWidth = nullptr;
    std::atomic<float>* pitchRange = nullptr;
    std::atomic<float>* pitchBipolar = nullptr;
    std::atomic<float>* freeRate = nullptr;
    std::atomic<float>* rateMs = nullptr;
    std::atomic<float>* rateDivision = nullptr;
    std::atomic<float>* rateOverlap = nullptr;

    // Audio thread only; keep the last values the host reported
    double hostBpm = TempoSync::fallbackBpm;
    double hostBeatsPerBar = 4.0; // quarter-note beats per bar, from the time signature

    std::atomic<float> uiBpm { (float) TempoSync::fallbackBpm };
    std::atomic<float> uiBeatsPerBar { 4.0f };
    std::atomic<float> uiPlayhead { -1.0f };

    CurveSlot volumeCurve { EnvelopeCurve {} };
    CurveSlot pitchCurve { defaultPitchCurve() };
    std::atomic<int> curveVersion { 0 };
    std::atomic<float> effectiveSliceMs { 500.0f }; // what the engine is using, for the UI
    std::atomic<float> effectiveIntervalMs { 500.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
