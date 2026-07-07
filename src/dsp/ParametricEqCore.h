#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>

namespace eq
{
enum class FilterType
{
    Bell = 0,
    LowShelf,
    HighShelf,
    LowCut,
    HighCut,
    Notch,
    BandPass
};

struct BandSettings
{
    bool enabled = true;
    FilterType type = FilterType::Bell;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
};

template <std::size_t BandCount>
inline std::array<BandSettings, BandCount> defaultBandSettings()
{
    static constexpr std::array<double, 6> anchorFrequencies { 80.0, 180.0, 600.0, 1500.0, 4000.0, 10000.0 };

    std::array<BandSettings, BandCount> bands {};

    for (std::size_t index = 0; index < BandCount; ++index)
    {
        auto& band = bands[index];
        band.enabled = true;
        band.type = FilterType::Bell;
        band.frequencyHz = index < anchorFrequencies.size() ? anchorFrequencies[index] : 1000.0;
        band.gainDb = 0.0;
        band.q = 1.0;
    }

    return bands;
}

struct BiquadCoefficients
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

inline double clampFrequency(double frequencyHz, double sampleRate)
{
    return std::clamp(frequencyHz, 10.0, (sampleRate * 0.5) - 10.0);
}

inline double clampQ(double q)
{
    return std::clamp(q, 0.1, 24.0);
}

inline BiquadCoefficients normalize(double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

inline BiquadCoefficients designBiquad(const BandSettings& inputSettings, double sampleRate)
{
    if (!inputSettings.enabled)
        return {};

    auto settings = inputSettings;
    settings.frequencyHz = clampFrequency(settings.frequencyHz, sampleRate);
    settings.q = clampQ(settings.q);

    const auto omega = 2.0 * M_PI * settings.frequencyHz / sampleRate;
    const auto sinOmega = std::sin(omega);
    const auto cosOmega = std::cos(omega);
    const auto alpha = sinOmega / (2.0 * settings.q);
    const auto amplitude = std::pow(10.0, settings.gainDb / 40.0);
    const auto sqrtA = std::sqrt(amplitude);

    switch (settings.type)
    {
        case FilterType::Bell:
        {
            return normalize(
                1.0 + alpha * amplitude,
                -2.0 * cosOmega,
                1.0 - alpha * amplitude,
                1.0 + alpha / amplitude,
                -2.0 * cosOmega,
                1.0 - alpha / amplitude);
        }

        case FilterType::LowShelf:
        {
            const auto shelfSlope = std::clamp(settings.q, 0.1, 1.0);
            const auto shelfAlpha = sinOmega / 2.0 * std::sqrt((amplitude + 1.0 / amplitude) * (1.0 / shelfSlope - 1.0) + 2.0);
            const auto twoSqrtAAlpha = 2.0 * sqrtA * shelfAlpha;

            return normalize(
                amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosOmega + twoSqrtAAlpha),
                2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosOmega),
                amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosOmega - twoSqrtAAlpha),
                (amplitude + 1.0) + (amplitude - 1.0) * cosOmega + twoSqrtAAlpha,
                -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosOmega),
                (amplitude + 1.0) + (amplitude - 1.0) * cosOmega - twoSqrtAAlpha);
        }

        case FilterType::HighShelf:
        {
            const auto shelfSlope = std::clamp(settings.q, 0.1, 1.0);
            const auto shelfAlpha = sinOmega / 2.0 * std::sqrt((amplitude + 1.0 / amplitude) * (1.0 / shelfSlope - 1.0) + 2.0);
            const auto twoSqrtAAlpha = 2.0 * sqrtA * shelfAlpha;

            return normalize(
                amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosOmega + twoSqrtAAlpha),
                -2.0 * amplitude * ((amplitude - 1.0) + (amplitude + 1.0) * cosOmega),
                amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosOmega - twoSqrtAAlpha),
                (amplitude + 1.0) - (amplitude - 1.0) * cosOmega + twoSqrtAAlpha,
                2.0 * ((amplitude - 1.0) - (amplitude + 1.0) * cosOmega),
                (amplitude + 1.0) - (amplitude - 1.0) * cosOmega - twoSqrtAAlpha);
        }

        case FilterType::LowCut:
        {
            return normalize(
                (1.0 + cosOmega) * 0.5,
                -(1.0 + cosOmega),
                (1.0 + cosOmega) * 0.5,
                1.0 + alpha,
                -2.0 * cosOmega,
                1.0 - alpha);
        }

        case FilterType::HighCut:
        {
            return normalize(
                (1.0 - cosOmega) * 0.5,
                1.0 - cosOmega,
                (1.0 - cosOmega) * 0.5,
                1.0 + alpha,
                -2.0 * cosOmega,
                1.0 - alpha);
        }

        case FilterType::Notch:
        {
            return normalize(
                1.0,
                -2.0 * cosOmega,
                1.0,
                1.0 + alpha,
                -2.0 * cosOmega,
                1.0 - alpha);
        }

        case FilterType::BandPass:
        {
            return normalize(
                alpha,
                0.0,
                -alpha,
                1.0 + alpha,
                -2.0 * cosOmega,
                1.0 - alpha);
        }
    }

    return {};
}

inline double magnitudeAt(const BiquadCoefficients& coefficients, double frequencyHz, double sampleRate)
{
    const auto omega = 2.0 * M_PI * clampFrequency(frequencyHz, sampleRate) / sampleRate;
    const std::complex<double> z1 = std::exp(std::complex<double>(0.0, -omega));
    const auto z2 = z1 * z1;

    const auto numerator = coefficients.b0 + coefficients.b1 * z1 + coefficients.b2 * z2;
    const auto denominator = 1.0 + coefficients.a1 * z1 + coefficients.a2 * z2;

    return std::abs(numerator / denominator);
}

class Biquad
{
public:
    void setCoefficients(BiquadCoefficients newCoefficients)
    {
        coefficients = newCoefficients;
    }

    void reset()
    {
        z1 = 0.0;
        z2 = 0.0;
    }

    float process(float input)
    {
        const auto output = coefficients.b0 * input + z1;
        z1 = coefficients.b1 * input - coefficients.a1 * output + z2;
        z2 = coefficients.b2 * input - coefficients.a2 * output;
        return static_cast<float>(output);
    }

private:
    BiquadCoefficients coefficients;
    double z1 = 0.0;
    double z2 = 0.0;
};

template <std::size_t BandCount>
class MonoEq
{
public:
    void reset()
    {
        for (auto& filter : filters)
            filter.reset();
    }

    void setBand(std::size_t index, const BandSettings& settings, double sampleRate)
    {
        if (index >= filters.size())
            return;

        filters[index].setCoefficients(designBiquad(settings, sampleRate));
    }

    float process(float input)
    {
        auto output = input;
        for (auto& filter : filters)
            output = filter.process(output);

        return output;
    }

private:
    std::array<Biquad, BandCount> filters;
};
}
