#pragma once

#include "TempoSync.h"
#include "dsp/DelayEngine.h"
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

    // The delay buffer is sized for this. Synced delays can exceed the 2 s ms range:
    // 8 s covers a 1/1 division down to 30 BPM; anything longer is clamped.
    static constexpr double kMaxDelaySeconds = 8.0;

private:
    void pushParametersToEngine();
    float computeDelayTimeMs();
    void updateHostBpm();
    void copyMonoInputToStereo (juce::AudioBuffer<float>& buffer) const;

    juce::AudioProcessorValueTreeState apvts;
    DelayEngine delayEngine;

    // Cached for lock-free reads on the audio thread
    std::atomic<float>* delayTimeMs = nullptr;
    std::atomic<float>* feedbackPercent = nullptr;
    std::atomic<float>* mixPercent = nullptr;
    std::atomic<float>* pingPong = nullptr;
    std::atomic<float>* sync = nullptr;
    std::atomic<float>* division = nullptr;

    double hostBpm = TempoSync::fallbackBpm; // audio thread only; keeps the last tempo the host reported
    std::atomic<float> effectiveDelayMs { 500.0f }; // what the engine is using, for the tail length

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
