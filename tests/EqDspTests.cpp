#include "dsp/ParametricEqCore.h"
#include "dsp/EqInteractionModel.h"
#include "dsp/RecentSampleFifo.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
constexpr double sampleRate = 48000.0;

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

double db(double magnitude)
{
    return 20.0 * std::log10(std::max(1.0e-12, magnitude));
}
}

int main()
{
    using namespace eq;

    {
        const auto normalized = normalizedXForFrequency(1000.0f);
        const auto roundTripped = frequencyForNormalizedX(normalized);
        require(std::abs(roundTripped - 1000.0f) < 0.5f, "frequency mapping should round-trip on log scale");
    }

    {
        const auto shelfQ = nextQFromWheel(FilterType::LowShelf, 0.95f, 1.0f);
        const auto bellQ = nextQFromWheel(FilterType::Bell, 0.95f, 1.0f);

        require(shelfQ <= 1.0f, "shelf wheel interaction should not exceed the effective shelf slope range");
        require(bellQ > 1.0f, "bell wheel interaction should allow narrow Q values above 1");
    }

    {
        RecentSampleFifo<8, 4> fifo;

        for (int sample = 0; sample < 12; ++sample)
            fifo.push(static_cast<float>(sample));

        std::array<float, 4> block {};
        require(fifo.pull(block), "recent sample FIFO should provide a block after overflow");
        require(block[0] == 8.0f && block[1] == 9.0f && block[2] == 10.0f && block[3] == 11.0f,
            "recent sample FIFO should keep the newest samples after overflow");
    }

    {
        const auto bands = defaultBandSettings<6>();

        for (auto frequency : { 30.0, 100.0, 1000.0, 10000.0, 18000.0 })
        {
            auto magnitude = 1.0;

            for (const auto& band : bands)
                magnitude *= magnitudeAt(designBiquad(band, sampleRate), frequency, sampleRate);

            require(std::abs(db(magnitude)) < 0.01, "default EQ should be neutral across the audible spectrum");
        }
    }

    {
        BandSettings band;
        band.type = FilterType::Bell;
        band.frequencyHz = 1000.0;
        band.gainDb = 6.0;
        band.q = 1.0;

        const auto coefficients = designBiquad(band, sampleRate);
        const auto centerDb = db(magnitudeAt(coefficients, band.frequencyHz, sampleRate));
        const auto lowDb = db(magnitudeAt(coefficients, 100.0, sampleRate));

        require(centerDb > 5.5 && centerDb < 6.5, "bell boost should be about +6 dB at center frequency");
        require(std::abs(lowDb) < 0.5, "bell boost should leave distant lows close to neutral");
    }

    {
        BandSettings band;
        band.type = FilterType::LowCut;
        band.frequencyHz = 120.0;
        band.gainDb = 0.0;
        band.q = 0.707;

        const auto coefficients = designBiquad(band, sampleRate);
        const auto lowDb = db(magnitudeAt(coefficients, 40.0, sampleRate));
        const auto passDb = db(magnitudeAt(coefficients, 1000.0, sampleRate));

        require(lowDb < -12.0, "low-cut should strongly attenuate sub bass");
        require(std::abs(passDb) < 0.25, "low-cut should keep mids close to neutral");
    }

    {
        BandSettings band;
        band.type = FilterType::HighShelf;
        band.frequencyHz = 6000.0;
        band.gainDb = 4.0;
        band.q = 0.707;

        const auto coefficients = designBiquad(band, sampleRate);
        const auto highDb = db(magnitudeAt(coefficients, 12000.0, sampleRate));
        const auto lowDb = db(magnitudeAt(coefficients, 300.0, sampleRate));

        require(highDb > 3.0 && highDb < 4.8, "high shelf should boost upper spectrum");
        require(std::abs(lowDb) < 0.4, "high shelf should keep lows close to neutral");
    }

    std::cout << "eq_dsp_tests passed\n";
    return 0;
}
