#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>

class AIAgentParametricEQAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                      private juce::Timer
{
public:
    explicit AIAgentParametricEQAudioProcessorEditor(AIAgentParametricEQAudioProcessor&);
    ~AIAgentParametricEQAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    struct BandVisual
    {
        juce::Point<float> position;
        bool enabled = true;
        eq::FilterType type = eq::FilterType::Bell;
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 1.0f;
    };

    void timerCallback() override;
    void updateSpectrum();
    void drawGrid(juce::Graphics&, juce::Rectangle<float> bounds);
    void drawSpectrum(juce::Graphics&, juce::Rectangle<float> bounds);
    void drawResponse(juce::Graphics&, juce::Rectangle<float> bounds);
    void drawBands(juce::Graphics&, juce::Rectangle<float> bounds);
    void drawReadout(juce::Graphics&, juce::Rectangle<float> bounds);

    juce::Rectangle<float> graphBounds() const;
    std::array<BandVisual, AIAgentParametricEQAudioProcessor::bandCount> currentBandVisuals(juce::Rectangle<float> bounds) const;
    BandVisual currentBandVisual(int bandIndex, juce::Rectangle<float> bounds) const;

    float xForFrequency(float frequency, juce::Rectangle<float> bounds) const;
    float frequencyForX(float x, juce::Rectangle<float> bounds) const;
    float yForGain(float gainDb, juce::Rectangle<float> bounds) const;
    float gainForY(float y, juce::Rectangle<float> bounds) const;

    int findBandAt(juce::Point<float> point) const;
    void setFloatParameter(const juce::String& parameterId, float value);
    void setChoiceParameter(const juce::String& parameterId, int choiceIndex);
    float getFloatParameter(const juce::String& parameterId) const;
    void showTypeMenu(int bandIndex);
    juce::Colour colourForBand(int bandIndex) const;

    AIAgentParametricEQAudioProcessor& processorRef;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::array<float, AIAgentParametricEQAudioProcessor::analyzerBlockSize> analyzerBlock {};
    std::array<float, fftSize * 2> fftBuffer {};
    std::array<float, fftSize / 2> spectrumDb {};

    int selectedBand = -1;
    int hoveredBand = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIAgentParametricEQAudioProcessorEditor)
};
