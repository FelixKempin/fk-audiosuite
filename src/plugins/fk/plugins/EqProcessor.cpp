#include "fk/plugins/EqProcessor.h"

#include "fk/plugins/EqView.h"
#include "fk/plugins/i18n/Text.h"

#include <span>

namespace fk::plugins
{

namespace
{
constexpr auto highPassOnId = "hpOn";
constexpr auto highPassFreqId = "hpFreq";

struct BandDefaults
{
    const char* idPrefix;
    const char* label;
    dsp::filters::FilterType type;
    float frequencyHz;
};

constexpr std::array<BandDefaults, 4> bandDefaults {
    { { "low", NEEDS_TRANS ("Low Shelf"), dsp::filters::FilterType::lowShelf, 100.0f },
      { "lowMid", NEEDS_TRANS ("Low Mid"), dsp::filters::FilterType::peak, 400.0f },
      { "highMid", NEEDS_TRANS ("High Mid"), dsp::filters::FilterType::peak, 2500.0f },
      { "high", NEEDS_TRANS ("High Shelf"), dsp::filters::FilterType::highShelf, 8000.0f } }
};

juce::String freqId (const char* prefix)
{
    return juce::String (prefix) + "Freq";
}
juce::String gainId (const char* prefix)
{
    return juce::String (prefix) + "Gain";
}
juce::String qId (const char* prefix)
{
    return juce::String (prefix) + "Q";
}

juce::NormalisableRange<float> frequencyRange()
{
    // Logarithmisch: Die Mitte des Reglers liegt bei etwa 630 Hz, wie Musiker es erwarten.
    // Schrittweite 1 Hz, damit Anzeige und Eingabe ganze Hertz zeigen statt 99.9999924.
    juce::NormalisableRange<float> range (20.0f, 20000.0f, 1.0f);
    range.setSkewForCentre (630.0f);
    return range;
}
} // namespace

EqProcessor::EqProcessor()
    : InternalPlugin (info(), createLayout())
    , highPassEnabled (parameter (highPassOnId))
    , highPassFrequencyHz (parameter (highPassFreqId))
{
    for (int i = 0; i < numBands; ++i)
    {
        bands[static_cast<std::size_t> (i)].type = bandDefaults[static_cast<std::size_t> (i)].type;
        bands[static_cast<std::size_t> (i)].frequencyHz =
            &parameter (freqId (bandDefaults[static_cast<std::size_t> (i)].idPrefix));
        bands[static_cast<std::size_t> (i)].gainDb =
            &parameter (gainId (bandDefaults[static_cast<std::size_t> (i)].idPrefix));
        bands[static_cast<std::size_t> (i)].q = &parameter (qId (bandDefaults[static_cast<std::size_t> (i)].idPrefix));
    }
}

InternalPlugin::Info EqProcessor::info()
{
    return { "FK EQ", "EQ", 0x464b0002 };
}

juce::AudioProcessorValueTreeState::ParameterLayout EqProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { highPassOnId, 1 }, TRANS ("HP On"), false));
    layout.add (
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { highPassFreqId, 1 },
                                                     TRANS ("HP Freq"),
                                                     juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.4f),
                                                     80.0f,
                                                     juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    for (const auto& band : bandDefaults)
    {
        const auto label = juce::translate (band.label);
        layout.add (
            std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { freqId (band.idPrefix), 1 },
                                                         i18n::format (TRANS ("{band} Freq"), { { "band", label } }),
                                                         frequencyRange(),
                                                         band.frequencyHz,
                                                         juce::AudioParameterFloatAttributes().withLabel ("Hz")));
        layout.add (
            std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { gainId (band.idPrefix), 1 },
                                                         i18n::format (TRANS ("{band} Gain"), { { "band", label } }),
                                                         juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f),
                                                         0.0f,
                                                         juce::AudioParameterFloatAttributes().withLabel ("dB")));
        layout.add (
            std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { qId (band.idPrefix), 1 },
                                                         i18n::format (TRANS ("{band} Q"), { { "band", label } }),
                                                         juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f),
                                                         0.707f));
    }

    return layout;
}

void EqProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate.store (sampleRate);
    spectrumAnalyser.prepare (sampleRate);

    for (auto& filter : highPassFilters)
        filter.reset();

    for (auto& band : bands)
    {
        band.lastFrequency = -1.0f; // erzwingt Neuberechnung

        for (auto& filter : band.filters)
            filter.reset();
    }

    updateCoefficientsIfNeeded();
}

void EqProcessor::releaseResources() {}

void EqProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateCoefficientsIfNeeded();

    const auto numSamples = static_cast<std::size_t> (buffer.getNumSamples());
    const bool highPassOn = highPassEnabled.load() > 0.5f;

    for (int channel = 0; channel < juce::jmin (numChannels, buffer.getNumChannels()); ++channel)
    {
        const std::span<float> samples (buffer.getWritePointer (channel), numSamples);
        const auto channelIndex = static_cast<std::size_t> (channel);

        if (highPassOn)
            highPassFilters[channelIndex].process (samples);

        for (auto& band : bands)
            band.filters[channelIndex].process (samples);
    }

    spectrumAnalyser.push (buffer); // Ausgangssignal, damit Spektrum und Kurve zusammenpassen
}

void EqProcessor::updateCoefficientsIfNeeded()
{
    using Coefficients = dsp::filters::Biquad<float>::Coefficients;

    const float highPassFrequency = highPassFrequencyHz.load();

    if (highPassFrequency != lastHighPassFrequency)
    {
        lastHighPassFrequency = highPassFrequency;
        const auto coefficients = dsp::filters::Biquad<float>::makeCoefficients (
            dsp::filters::FilterType::highPass, currentSampleRate.load(), highPassFrequency, 0.707);

        for (auto& filter : highPassFilters)
            filter.setCoefficients (coefficients);
    }

    for (auto& band : bands)
    {
        const float frequency = band.frequencyHz->load();
        const float gain = band.gainDb->load();
        const float q = band.q->load();

        if (frequency == band.lastFrequency && gain == band.lastGain && q == band.lastQ)
            continue;

        band.lastFrequency = frequency;
        band.lastGain = gain;
        band.lastQ = q;

        const Coefficients coefficients =
            dsp::filters::Biquad<float>::makeCoefficients (band.type, currentSampleRate.load(), frequency, q, gain);

        for (auto& filter : band.filters)
            filter.setCoefficients (coefficients);
    }
}

std::vector<EqProcessor::BandDescription> EqProcessor::describeBands()
{
    std::vector<BandDescription> result;
    result.push_back ({ .frequencyId = highPassFreqId, .shortLabel = "HP" });

    int number = 1;
    for (const auto& band : bandDefaults)
        result.push_back ({ .frequencyId = freqId (band.idPrefix),
                            .gainId = gainId (band.idPrefix),
                            .qId = qId (band.idPrefix),
                            .shortLabel = juce::String (number++) });

    return result;
}

float EqProcessor::getMagnitudeDb (float frequencyHz) const
{
    using Biquad = dsp::filters::Biquad<float>;
    const double sampleRate = currentSampleRate.load();
    const double hz = juce::jlimit (1.0, sampleRate * 0.49, static_cast<double> (frequencyHz));
    double total = 0.0;

    if (highPassEnabled.load() > 0.5f)
        total +=
            Biquad::magnitudeDb (Biquad::makeCoefficients (
                                     dsp::filters::FilterType::highPass, sampleRate, highPassFrequencyHz.load(), 0.707),
                                 hz,
                                 sampleRate);

    for (const auto& band : bands)
        total += Biquad::magnitudeDb (
            Biquad::makeCoefficients (
                band.type, sampleRate, band.frequencyHz->load(), band.q->load(), band.gainDb->load()),
            hz,
            sampleRate);

    return static_cast<float> (total);
}

analysis::SpectrumAnalyser& EqProcessor::getSpectrumAnalyser() noexcept
{
    return spectrumAnalyser;
}

ui::PanelDescription EqProcessor::describePanel()
{
    ui::PanelDescription description { .title = info().name };

    // Die Szene oben: Spektrum, Kurve, Griffe. Die Regler darunter bleiben der genaue Weg.
    // Die Fabrik fängt this: Der Editor lebt nie länger als sein Prozessor (JUCE-Vertrag).
    description.sections.push_back (
        { .title = TRANS ("Frequency response"), .scene = [this] { return std::make_unique<EqView> (*this); }, .sceneHeight = 240 });
    description.sections.push_back (
        { .title = TRANS ("High-pass"), .controls = { ui::knob (highPassOnId), ui::knob (highPassFreqId) } });

    for (const auto& band : bandDefaults)
        description.sections.push_back ({ .title = juce::translate (band.label),
                                          .controls = { ui::knob (freqId (band.idPrefix)),
                                                        ui::knob (gainId (band.idPrefix)),
                                                        ui::knob (qId (band.idPrefix)) } });

    return description;
}

} // namespace fk::plugins
