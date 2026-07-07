#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float minGain = -18.0f;
constexpr float maxGain = 18.0f;
constexpr float minQ = 0.1f;
constexpr float maxQ = 24.0f;
}

AIAgentParametricEQAudioProcessor::AIAgentParametricEQAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    cacheParameterPointers();
}

void AIAgentParametricEQAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    analyzerFifo.reset();

    for (auto& eqChannel : channelEqs)
        eqChannel.reset();

    updateFilters();
}

void AIAgentParametricEQAudioProcessor::releaseResources()
{
}

bool AIAgentParametricEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    return input == output && (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo());
}

void AIAgentParametricEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateFilters();

    const auto channelsToProcess = std::min(buffer.getNumChannels(), static_cast<int>(channelEqs.size()));

    for (int channel = 0; channel < channelsToProcess; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            channelData[sample] = channelEqs[static_cast<std::size_t>(channel)].process(channelData[sample]);
    }

    if (buffer.getNumChannels() > 0)
    {
        const auto* left = buffer.getReadPointer(0);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            analyzerFifo.push(left[sample]);
    }
}

juce::AudioProcessorEditor* AIAgentParametricEQAudioProcessor::createEditor()
{
    return new AIAgentParametricEQAudioProcessorEditor(*this);
}

void AIAgentParametricEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AIAgentParametricEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

bool AIAgentParametricEQAudioProcessor::pullAnalyzerBlock(std::array<float, analyzerBlockSize>& target)
{
    return analyzerFifo.pull(target);
}

juce::AudioProcessorValueTreeState::ParameterLayout AIAgentParametricEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    const auto typeNames = filterTypeNames();

    for (int band = 0; band < bandCount; ++band)
    {
        const auto defaultBands = eq::defaultBandSettings<bandCount>();
        const auto& defaultBand = defaultBands[static_cast<std::size_t>(band)];
        const auto bandNumber = band + 1;
        const auto bandName = "Band " + juce::String(bandNumber);

        layout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(bandParameterId(band, "enabled"), 1), bandName + " Enabled", defaultBand.enabled));

        layout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(bandParameterId(band, "type"), 1), bandName + " Type", typeNames, static_cast<int>(defaultBand.type)));

        juce::NormalisableRange<float> frequencyRange(minFrequency, maxFrequency, 0.0f, 0.25f);
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParameterId(band, "freq"), 1), bandName + " Frequency", frequencyRange, static_cast<float>(defaultBand.frequencyHz)));

        juce::NormalisableRange<float> gainRange(minGain, maxGain);
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParameterId(band, "gain"), 1), bandName + " Gain", gainRange, static_cast<float>(defaultBand.gainDb)));

        juce::NormalisableRange<float> qRange(minQ, maxQ, 0.0f, 0.35f);
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParameterId(band, "q"), 1), bandName + " Q", qRange, static_cast<float>(defaultBand.q)));
    }

    return { layout.begin(), layout.end() };
}

juce::String AIAgentParametricEQAudioProcessor::bandParameterId(int bandIndex, const juce::String& suffix)
{
    return "band" + juce::String(bandIndex + 1) + "_" + suffix;
}

juce::StringArray AIAgentParametricEQAudioProcessor::filterTypeNames()
{
    return { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass" };
}

eq::FilterType AIAgentParametricEQAudioProcessor::filterTypeFromIndex(int index)
{
    switch (index)
    {
        case 1: return eq::FilterType::LowShelf;
        case 2: return eq::FilterType::HighShelf;
        case 3: return eq::FilterType::LowCut;
        case 4: return eq::FilterType::HighCut;
        case 5: return eq::FilterType::Notch;
        case 6: return eq::FilterType::BandPass;
        case 0:
        default: return eq::FilterType::Bell;
    }
}

void AIAgentParametricEQAudioProcessor::cacheParameterPointers()
{
    for (int band = 0; band < bandCount; ++band)
    {
        auto& refs = bandParams[static_cast<std::size_t>(band)];
        refs.enabled = parameters.getRawParameterValue(bandParameterId(band, "enabled"));
        refs.type = parameters.getRawParameterValue(bandParameterId(band, "type"));
        refs.frequency = parameters.getRawParameterValue(bandParameterId(band, "freq"));
        refs.gain = parameters.getRawParameterValue(bandParameterId(band, "gain"));
        refs.q = parameters.getRawParameterValue(bandParameterId(band, "q"));
    }
}

void AIAgentParametricEQAudioProcessor::updateFilters()
{
    for (int band = 0; band < bandCount; ++band)
    {
        const auto settings = getBandSettings(band);

        for (auto& eqChannel : channelEqs)
            eqChannel.setBand(static_cast<std::size_t>(band), settings, currentSampleRate);
    }
}

eq::BandSettings AIAgentParametricEQAudioProcessor::getBandSettings(int bandIndex) const
{
    const auto& refs = bandParams[static_cast<std::size_t>(bandIndex)];

    eq::BandSettings settings;
    settings.enabled = refs.enabled != nullptr && refs.enabled->load() > 0.5f;
    settings.type = filterTypeFromIndex(refs.type != nullptr ? static_cast<int>(std::round(refs.type->load())) : 0);
    settings.frequencyHz = refs.frequency != nullptr ? refs.frequency->load() : 1000.0f;
    settings.gainDb = refs.gain != nullptr ? refs.gain->load() : 0.0f;
    settings.q = refs.q != nullptr ? refs.q->load() : 1.0f;

    if (settings.type == eq::FilterType::LowCut || settings.type == eq::FilterType::HighCut
        || settings.type == eq::FilterType::Notch || settings.type == eq::FilterType::BandPass)
        settings.gainDb = 0.0;

    return settings;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AIAgentParametricEQAudioProcessor();
}
