#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "GlideEngine.h"

class PedalSteelGlideProcessor : public juce::AudioProcessor
{
public:
    PedalSteelGlideProcessor();
    ~PedalSteelGlideProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    GlideParams currentParams() const;
    void run (juce::MidiBuffer& midi, int numSamples);

    GlideEngine engine;
    double sr = 48000.0;

    std::atomic<float>* pPoly = nullptr;
    std::atomic<float>* pMaster = nullptr;
    std::atomic<float>* pLegato = nullptr;
    std::atomic<float>* pGlideMin = nullptr;
    std::atomic<float>* pGlideMax = nullptr;
    std::atomic<float>* pCurve = nullptr;
    std::atomic<float>* pBendRange = nullptr;
    std::atomic<float>* pWheelRange = nullptr;
    std::atomic<float>* pVibRate = nullptr;
    std::atomic<float>* pVibDepth = nullptr;
    std::atomic<float>* pVibRandom = nullptr;
    std::atomic<float>* pVibSource = nullptr;
    std::atomic<float>* pCC7 = nullptr;
    std::atomic<float>* pAutoRange = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalSteelGlideProcessor)
};
