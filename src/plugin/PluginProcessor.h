#pragma once

#include <JuceHeader.h>

#include "dsp/ParametricEqCore.h"
#include "dsp/RecentSampleFifo.h"

#include <array>

class AIAgentParametricEQAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int bandCount = 6;
    static constexpr int analyzerFifoCapacity = 32768;
    static constexpr int analyzerBlockSize = 2048;

    AIAgentParametricEQAudioProcessor();
    ~AIAgentParametricEQAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool pullAnalyzerBlock(std::array<float, analyzerBlockSize>& target);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String bandParameterId(int bandIndex, const juce::String& suffix);
    static juce::StringArray filterTypeNames();
    static eq::FilterType filterTypeFromIndex(int index);

    juce::AudioProcessorValueTreeState parameters;

private:
    struct BandParamRefs
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* frequency = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
    };

    void cacheParameterPointers();
    void updateFilters();
    eq::BandSettings getBandSettings(int bandIndex) const;

    std::array<BandParamRefs, bandCount> bandParams {};
    std::array<eq::MonoEq<bandCount>, 2> channelEqs {};
    eq::RecentSampleFifo<analyzerFifoCapacity, analyzerBlockSize> analyzerFifo;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIAgentParametricEQAudioProcessor)
};
