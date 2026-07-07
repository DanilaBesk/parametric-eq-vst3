#include "PluginEditor.h"
#include "dsp/EqInteractionModel.h"

namespace
{
const std::array<float, 10> frequencyMarkers { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
const std::array<float, 7> gainMarkers { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f };

juce::String formatFrequency(float frequency)
{
    if (frequency >= 1000.0f)
        return juce::String(frequency / 1000.0f, frequency >= 10000.0f ? 0 : 1) + "k";

    return juce::String(static_cast<int>(std::round(frequency)));
}

juce::String filterTypeLabel(eq::FilterType type)
{
    switch (type)
    {
        case eq::FilterType::LowShelf: return "Shelf L";
        case eq::FilterType::HighShelf: return "Shelf H";
        case eq::FilterType::LowCut: return "Low Cut";
        case eq::FilterType::HighCut: return "High Cut";
        case eq::FilterType::Notch: return "Notch";
        case eq::FilterType::BandPass: return "Band";
        case eq::FilterType::Bell:
        default: return "Bell";
    }
}
}

AIAgentParametricEQAudioProcessorEditor::AIAgentParametricEQAudioProcessorEditor(AIAgentParametricEQAudioProcessor& audioProcessor)
    : AudioProcessorEditor(&audioProcessor),
      processorRef(audioProcessor)
{
    spectrumDb.fill(-100.0f);
    setSize(940, 560);
    setResizable(true, true);
    setResizeLimits(720, 420, 1600, 980);
    startTimerHz(30);
}

void AIAgentParametricEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101214));

    const auto bounds = graphBounds();
    drawGrid(g, bounds);
    drawSpectrum(g, bounds);
    drawResponse(g, bounds);
    drawBands(g, bounds);
    drawReadout(g, bounds);
}

void AIAgentParametricEQAudioProcessorEditor::resized()
{
}

void AIAgentParametricEQAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto band = findBandAt(event.position);

    if (event.mods.isPopupMenu() && band >= 0)
    {
        selectedBand = band;
        showTypeMenu(band);
        return;
    }

    selectedBand = band;
    repaint();
}

void AIAgentParametricEQAudioProcessorEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (selectedBand < 0)
        return;

    const auto bounds = graphBounds();
    const auto frequency = frequencyForX(event.position.x, bounds);
    auto gain = gainForY(event.position.y, bounds);
    const auto typeIndex = static_cast<int>(std::round(getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(selectedBand, "type"))));
    const auto type = AIAgentParametricEQAudioProcessor::filterTypeFromIndex(typeIndex);

    if (type == eq::FilterType::LowCut || type == eq::FilterType::HighCut
        || type == eq::FilterType::Notch || type == eq::FilterType::BandPass)
        gain = 0.0f;

    setFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(selectedBand, "freq"), frequency);
    setFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(selectedBand, "gain"), gain);
}

void AIAgentParametricEQAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
}

void AIAgentParametricEQAudioProcessorEditor::mouseMove(const juce::MouseEvent& event)
{
    const auto newHoveredBand = findBandAt(event.position);

    if (newHoveredBand != hoveredBand)
    {
        hoveredBand = newHoveredBand;
        repaint();
    }
}

void AIAgentParametricEQAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto band = findBandAt(event.position);

    if (band < 0)
        return;

    const auto parameterId = AIAgentParametricEQAudioProcessor::bandParameterId(band, "q");
    const auto typeIndex = static_cast<int>(std::round(getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(band, "type"))));
    const auto type = AIAgentParametricEQAudioProcessor::filterTypeFromIndex(typeIndex);
    const auto currentQ = getFloatParameter(parameterId);
    const auto nextQ = eq::nextQFromWheel(type, currentQ, wheel.deltaY);
    setFloatParameter(parameterId, nextQ);
    selectedBand = band;
}

void AIAgentParametricEQAudioProcessorEditor::timerCallback()
{
    updateSpectrum();
    repaint();
}

void AIAgentParametricEQAudioProcessorEditor::updateSpectrum()
{
    while (processorRef.pullAnalyzerBlock(analyzerBlock))
    {
        std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
        std::copy(analyzerBlock.begin(), analyzerBlock.end(), fftBuffer.begin());
        window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftBuffer.data());

        for (std::size_t bin = 1; bin < spectrumDb.size(); ++bin)
        {
            const auto magnitude = fftBuffer[bin] / static_cast<float>(fftSize);
            const auto db = juce::Decibels::gainToDecibels(magnitude, -100.0f);
            spectrumDb[bin] = 0.72f * spectrumDb[bin] + 0.28f * juce::jlimit(-100.0f, 0.0f, db + 18.0f);
        }
    }
}

void AIAgentParametricEQAudioProcessorEditor::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colour(0xff181d20));
    g.fillRect(bounds);

    g.setColour(juce::Colour(0xff273039));
    for (auto frequency : frequencyMarkers)
    {
        const auto x = xForFrequency(frequency, bounds);
        g.drawVerticalLine(static_cast<int>(std::round(x)), bounds.getY(), bounds.getBottom());
    }

    for (auto gain : gainMarkers)
    {
        const auto y = yForGain(gain, bounds);
        g.setColour(gain == 0.0f ? juce::Colour(0xff52606d) : juce::Colour(0xff273039));
        g.drawHorizontalLine(static_cast<int>(std::round(y)), bounds.getX(), bounds.getRight());
    }

    g.setColour(juce::Colour(0xff8f9ba5));
    g.setFont(12.0f);
    for (auto frequency : frequencyMarkers)
    {
        const auto x = xForFrequency(frequency, bounds);
        g.drawText(formatFrequency(frequency), juce::Rectangle<float>(x - 24.0f, bounds.getBottom() + 5.0f, 48.0f, 18.0f), juce::Justification::centred);
    }

    g.setColour(juce::Colour(0xff77838f));
    for (auto gain : gainMarkers)
    {
        const auto y = yForGain(gain, bounds);
        g.drawText((gain > 0.0f ? "+" : "") + juce::String(static_cast<int>(gain)), juce::Rectangle<float>(bounds.getRight() - 42.0f, y - 9.0f, 38.0f, 18.0f), juce::Justification::centredRight);
    }
}

void AIAgentParametricEQAudioProcessorEditor::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::Path spectrumPath;
    spectrumPath.startNewSubPath(bounds.getX(), bounds.getBottom());

    for (std::size_t bin = 1; bin < spectrumDb.size(); ++bin)
    {
        const auto sampleRate = processorRef.getSampleRate() > 0.0 ? processorRef.getSampleRate() : 48000.0;
        const auto frequency = static_cast<float>(bin) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
        if (frequency < eq::minUiFrequency || frequency > eq::maxUiFrequency)
            continue;

        const auto x = xForFrequency(frequency, bounds);
        const auto normalized = juce::jmap(spectrumDb[bin], -100.0f, 0.0f, 1.0f, 0.0f);
        const auto y = bounds.getY() + normalized * bounds.getHeight();
        spectrumPath.lineTo(x, y);
    }

    spectrumPath.lineTo(bounds.getRight(), bounds.getBottom());
    spectrumPath.closeSubPath();

    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0x6647c2ff), bounds.getCentreX(), bounds.getY(),
        juce::Colour(0x1019d18a), bounds.getCentreX(), bounds.getBottom(), false));
    g.fillPath(spectrumPath);
}

void AIAgentParametricEQAudioProcessorEditor::drawResponse(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::Path response;
    const auto sampleRate = processorRef.getSampleRate() > 0.0 ? processorRef.getSampleRate() : 48000.0;

    for (int pixel = 0; pixel < static_cast<int>(bounds.getWidth()); ++pixel)
    {
        const auto x = bounds.getX() + static_cast<float>(pixel);
        const auto frequency = frequencyForX(x, bounds);
        auto magnitude = 1.0;

        for (int band = 0; band < AIAgentParametricEQAudioProcessor::bandCount; ++band)
        {
            const auto typeIndex = static_cast<int>(std::round(getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(band, "type"))));
            eq::BandSettings settings;
            settings.enabled = getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(band, "enabled")) > 0.5f;
            settings.type = AIAgentParametricEQAudioProcessor::filterTypeFromIndex(typeIndex);
            settings.frequencyHz = getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(band, "freq"));
            settings.gainDb = getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(band, "gain"));
            settings.q = getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(band, "q"));

            if (settings.type == eq::FilterType::LowCut || settings.type == eq::FilterType::HighCut
                || settings.type == eq::FilterType::Notch || settings.type == eq::FilterType::BandPass)
                settings.gainDb = 0.0;

            magnitude *= eq::magnitudeAt(eq::designBiquad(settings, sampleRate), frequency, sampleRate);
        }

        const auto db = juce::jlimit(eq::minUiGainDb, eq::maxUiGainDb, juce::Decibels::gainToDecibels(static_cast<float>(magnitude), -72.0f));
        const auto y = yForGain(db, bounds);

        if (pixel == 0)
            response.startNewSubPath(x, y);
        else
            response.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xffffd166));
    g.strokePath(response, juce::PathStrokeType(2.4f));
}

void AIAgentParametricEQAudioProcessorEditor::drawBands(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto visuals = currentBandVisuals(bounds);

    for (int band = 0; band < static_cast<int>(visuals.size()); ++band)
    {
        const auto& visual = visuals[static_cast<std::size_t>(band)];
        const auto colour = colourForBand(band);
        const auto radius = band == selectedBand ? 8.5f : band == hoveredBand ? 7.8f : 6.8f;

        g.setColour(colour.withAlpha(0.18f));
        g.fillEllipse(visual.position.x - 18.0f, visual.position.y - 18.0f, 36.0f, 36.0f);

        g.setColour(colour);
        g.fillEllipse(visual.position.x - radius, visual.position.y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour(juce::Colour(0xff0b0d0f));
        g.drawEllipse(visual.position.x - radius, visual.position.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        g.setColour(juce::Colour(0xffe8edf2));
        g.setFont(11.0f);
        g.drawText(juce::String(band + 1), juce::Rectangle<float>(visual.position.x - 7.0f, visual.position.y - 7.0f, 14.0f, 14.0f), juce::Justification::centred);

        if (band == hoveredBand || band == selectedBand)
        {
            const auto label = filterTypeLabel(visual.type) + "  " + formatFrequency(visual.frequency) + "Hz  "
                + juce::String(visual.gain, 1) + "dB  Q " + juce::String(visual.q, 2);
            const auto labelBounds = juce::Rectangle<float>(visual.position.x - 88.0f, visual.position.y - 42.0f, 176.0f, 22.0f)
                .constrainedWithin(bounds.reduced(4.0f));

            g.setColour(juce::Colour(0xcc101418));
            g.fillRoundedRectangle(labelBounds, 5.0f);
            g.setColour(colour);
            g.drawRoundedRectangle(labelBounds, 5.0f, 1.0f);
            g.setColour(juce::Colour(0xffedf1f5));
            g.setFont(12.0f);
            g.drawText(label, labelBounds.reduced(6.0f, 0.0f), juce::Justification::centred);
        }
    }
}

void AIAgentParametricEQAudioProcessorEditor::drawReadout(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colour(0xffc8d1da));
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText("AI Agent Parametric EQ", bounds.removeFromTop(28.0f).reduced(12.0f, 0.0f), juce::Justification::centredLeft);

    g.setFont(12.0f);
    g.setColour(juce::Colour(0xff8794a0));
    const auto sampleRate = processorRef.getSampleRate() > 0.0 ? processorRef.getSampleRate() : 48000.0;
    g.drawText(juce::String(sampleRate / 1000.0, 1) + " kHz", getLocalBounds().toFloat().reduced(16.0f).removeFromTop(24.0f), juce::Justification::centredRight);
}

juce::Rectangle<float> AIAgentParametricEQAudioProcessorEditor::graphBounds() const
{
    return getLocalBounds().toFloat().reduced(16.0f, 18.0f).withTrimmedTop(28.0f).withTrimmedBottom(28.0f);
}

std::array<AIAgentParametricEQAudioProcessorEditor::BandVisual, AIAgentParametricEQAudioProcessor::bandCount>
AIAgentParametricEQAudioProcessorEditor::currentBandVisuals(juce::Rectangle<float> bounds) const
{
    std::array<BandVisual, AIAgentParametricEQAudioProcessor::bandCount> visuals {};

    for (int band = 0; band < static_cast<int>(visuals.size()); ++band)
        visuals[static_cast<std::size_t>(band)] = currentBandVisual(band, bounds);

    return visuals;
}

AIAgentParametricEQAudioProcessorEditor::BandVisual
AIAgentParametricEQAudioProcessorEditor::currentBandVisual(int bandIndex, juce::Rectangle<float> bounds) const
{
    BandVisual visual;
    visual.enabled = getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(bandIndex, "enabled")) > 0.5f;
    visual.frequency = getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(bandIndex, "freq"));
    visual.gain = getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(bandIndex, "gain"));
    visual.q = getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(bandIndex, "q"));
    visual.type = AIAgentParametricEQAudioProcessor::filterTypeFromIndex(
        static_cast<int>(std::round(getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(bandIndex, "type")))));

    const auto pointGain = (visual.type == eq::FilterType::LowCut || visual.type == eq::FilterType::HighCut
                            || visual.type == eq::FilterType::Notch || visual.type == eq::FilterType::BandPass)
        ? 0.0f
        : visual.gain;

    visual.position = { xForFrequency(visual.frequency, bounds), yForGain(pointGain, bounds) };
    return visual;
}

float AIAgentParametricEQAudioProcessorEditor::xForFrequency(float frequency, juce::Rectangle<float> bounds) const
{
    return bounds.getX() + eq::normalizedXForFrequency(frequency) * bounds.getWidth();
}

float AIAgentParametricEQAudioProcessorEditor::frequencyForX(float x, juce::Rectangle<float> bounds) const
{
    const auto normalized = juce::jlimit(0.0f, 1.0f, (x - bounds.getX()) / bounds.getWidth());
    return eq::frequencyForNormalizedX(normalized);
}

float AIAgentParametricEQAudioProcessorEditor::yForGain(float gainDb, juce::Rectangle<float> bounds) const
{
    return bounds.getY() + eq::normalizedYForGain(gainDb) * bounds.getHeight();
}

float AIAgentParametricEQAudioProcessorEditor::gainForY(float y, juce::Rectangle<float> bounds) const
{
    const auto normalized = juce::jlimit(0.0f, 1.0f, (y - bounds.getY()) / bounds.getHeight());
    return eq::gainForNormalizedY(normalized);
}

int AIAgentParametricEQAudioProcessorEditor::findBandAt(juce::Point<float> point) const
{
    const auto visuals = currentBandVisuals(graphBounds());

    for (int band = static_cast<int>(visuals.size()) - 1; band >= 0; --band)
    {
        if (visuals[static_cast<std::size_t>(band)].position.getDistanceFrom(point) <= 15.0f)
            return band;
    }

    return -1;
}

void AIAgentParametricEQAudioProcessorEditor::setFloatParameter(const juce::String& parameterId, float value)
{
    if (auto* parameter = processorRef.parameters.getParameter(parameterId))
    {
        const auto normalized = parameter->convertTo0to1(value);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        parameter->endChangeGesture();
    }
}

void AIAgentParametricEQAudioProcessorEditor::setChoiceParameter(const juce::String& parameterId, int choiceIndex)
{
    setFloatParameter(parameterId, static_cast<float>(choiceIndex));
}

float AIAgentParametricEQAudioProcessorEditor::getFloatParameter(const juce::String& parameterId) const
{
    if (const auto* value = processorRef.parameters.getRawParameterValue(parameterId))
        return value->load();

    return 0.0f;
}

void AIAgentParametricEQAudioProcessorEditor::showTypeMenu(int bandIndex)
{
    juce::PopupMenu menu;
    const auto names = AIAgentParametricEQAudioProcessor::filterTypeNames();
    const auto currentType = static_cast<int>(std::round(getFloatParameter(AIAgentParametricEQAudioProcessor::bandParameterId(bandIndex, "type"))));

    for (int i = 0; i < names.size(); ++i)
        menu.addItem(i + 1, names[i], true, i == currentType);

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this, bandIndex](int result)
        {
            if (result > 0)
                setChoiceParameter(AIAgentParametricEQAudioProcessor::bandParameterId(bandIndex, "type"), result - 1);
        });
}

juce::Colour AIAgentParametricEQAudioProcessorEditor::colourForBand(int bandIndex) const
{
    static const std::array<juce::Colour, AIAgentParametricEQAudioProcessor::bandCount> colours {
        juce::Colour(0xff51d1f6),
        juce::Colour(0xff5ee0a1),
        juce::Colour(0xffffd166),
        juce::Colour(0xffff7a7a),
        juce::Colour(0xffb794ff),
        juce::Colour(0xfff28cc6)
    };

    return colours[static_cast<std::size_t>(bandIndex)];
}
