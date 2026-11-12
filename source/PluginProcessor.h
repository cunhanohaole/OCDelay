#pragma once

#include "TempoSync.h"
#include "curve/TripleBuffer.h"
#include "curve/VolumeCurve.h"
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

    static constexpr size_t kCurveTableSize = 2048;
    using CurveTable = std::array<float, kCurveTableSize>;

    // Curve editing: message thread only. updateCurve() publishes a new table to the audio
    // thread lock-free and bumps the version, so an open editor can notice session loads.
    const VolumeCurve& getCurve() const { return curve; }
    void updateCurve (const VolumeCurve& newCurve);
    int getCurveVersion() const { return curveVersion.load(); }

    // Host timing and playback position for the UI (written by the audio thread)
    float getHostBpm() const { return uiBpm.load(); }
    float getBeatsPerBar() const { return uiBeatsPerBar.load(); }
    float getPlayheadPosition() const { return uiPlayhead.load(); }
    float getSliceMs() const { return effectiveSliceMs.load(); }
    int getLengthBars() const;

    // Capture buffers are sized for this. Synced slices can exceed the 2 s ms range:
    // 8 s covers a 1/1 division down to 30 BPM; anything longer is clamped.
    static constexpr double kMaxSliceSeconds = 8.0;

private:
    void pushParametersToEngine();
    float computeSliceMs();
    double computeLengthSeconds (double bpm, double beatsPerBar) const;
    void updateHostTiming();
    static CurveTable makeCurveTable (const VolumeCurve& source);
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

    // Audio thread only; keep the last values the host reported
    double hostBpm = TempoSync::fallbackBpm;
    double hostBeatsPerBar = 4.0; // quarter-note beats per bar, from the time signature

    std::atomic<float> uiBpm { (float) TempoSync::fallbackBpm };
    std::atomic<float> uiBeatsPerBar { 4.0f };
    std::atomic<float> uiPlayhead { -1.0f };

    VolumeCurve curve;
    TripleBuffer<CurveTable> curveTables;
    std::atomic<int> curveVersion { 0 };
    std::atomic<float> effectiveSliceMs { 500.0f }; // what the engine is using, for the UI

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
