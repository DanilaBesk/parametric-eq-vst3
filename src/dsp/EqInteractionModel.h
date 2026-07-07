#pragma once

#include "dsp/ParametricEqCore.h"

#include <algorithm>
#include <cmath>

namespace eq
{
inline constexpr float minUiFrequency = 20.0f;
inline constexpr float maxUiFrequency = 20000.0f;
inline constexpr float minUiGainDb = -18.0f;
inline constexpr float maxUiGainDb = 18.0f;
inline constexpr float minUiQ = 0.1f;
inline constexpr float maxUiQ = 24.0f;

inline float normalizedXForFrequency(float frequency)
{
    const auto clampedFrequency = std::clamp(frequency, minUiFrequency, maxUiFrequency);
    return std::log(clampedFrequency / minUiFrequency) / std::log(maxUiFrequency / minUiFrequency);
}

inline float frequencyForNormalizedX(float normalizedX)
{
    const auto clampedX = std::clamp(normalizedX, 0.0f, 1.0f);
    return minUiFrequency * std::pow(maxUiFrequency / minUiFrequency, clampedX);
}

inline float normalizedYForGain(float gainDb)
{
    const auto clampedGain = std::clamp(gainDb, minUiGainDb, maxUiGainDb);
    return (maxUiGainDb - clampedGain) / (maxUiGainDb - minUiGainDb);
}

inline float gainForNormalizedY(float normalizedY)
{
    const auto clampedY = std::clamp(normalizedY, 0.0f, 1.0f);
    return maxUiGainDb + clampedY * (minUiGainDb - maxUiGainDb);
}

inline float maxQForType(FilterType type)
{
    return (type == FilterType::LowShelf || type == FilterType::HighShelf) ? 1.0f : maxUiQ;
}

inline float nextQFromWheel(FilterType type, float currentQ, float wheelDeltaY)
{
    const auto multiplier = std::pow(1.18f, wheelDeltaY * 8.0f);
    return std::clamp(currentQ * multiplier, minUiQ, maxQForType(type));
}
}
