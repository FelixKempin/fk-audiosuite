#include "fk/plugins/analysis/SpectrumAnalyser.h"

#include <algorithm>
#include <cmath>

namespace fk::plugins::analysis
{

SpectrumAnalyser::SpectrumAnalyser()
    : fifoBuffer (static_cast<std::size_t> (fifoCapacity), 0.0f)
    , fftData (static_cast<std::size_t> (fftSize) * 2, 0.0f)
    , magnitudesDb (static_cast<std::size_t> (numBins), minimumDb)
{
    pending.reserve (static_cast<std::size_t> (fifoCapacity));
}

void SpectrumAnalyser::prepare (double newSampleRate)
{
    sampleRate.store (newSampleRate);
    fifo.reset();
    pending.clear();
    std::ranges::fill (magnitudesDb, minimumDb);
}

void SpectrumAnalyser::push (const juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // AbstractFifo liefert bis zu zwei Bereiche (Ringpuffer-Umbruch); wir füllen beide.
    const auto scope = fifo.write (numSamples);
    const float scale = 1.0f / static_cast<float> (numChannels);

    auto writeRange = [&] (int start, int size, int sourceOffset)
    {
        for (int i = 0; i < size; ++i)
        {
            float sum = 0.0f;

            for (int channel = 0; channel < numChannels; ++channel)
                sum += buffer.getSample (channel, sourceOffset + i);

            fifoBuffer[static_cast<std::size_t> (start + i)] = sum * scale;
        }
    };

    writeRange (scope.startIndex1, scope.blockSize1, 0);
    writeRange (scope.startIndex2, scope.blockSize2, scope.blockSize1);
}

bool SpectrumAnalyser::update()
{
    const int available = fifo.getNumReady();

    if (available > 0)
    {
        const auto scope = fifo.read (available);
        pending.insert (pending.end(),
                        fifoBuffer.begin() + scope.startIndex1,
                        fifoBuffer.begin() + scope.startIndex1 + scope.blockSize1);
        pending.insert (pending.end(),
                        fifoBuffer.begin() + scope.startIndex2,
                        fifoBuffer.begin() + scope.startIndex2 + scope.blockSize2);
    }

    if (static_cast<int> (pending.size()) < fftSize)
        return false;

    // Die jüngsten fftSize Samples analysieren, ältere verwerfen: Die Anzeige soll "jetzt" zeigen.
    const auto begin = pending.end() - fftSize;
    std::ranges::fill (fftData, 0.0f);
    std::copy (begin, pending.end(), fftData.begin());
    pending.clear();

    window.multiplyWithWindowingTable (fftData.data(), static_cast<std::size_t> (fftSize));
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    // Normierung: Ein Vollaussteuerungs-Sinus ergibt im Bin seiner Frequenz etwa 0 dB. JUCE
    // normiert das Hann-Fenster auf Summe = fftSize, die FFT liefert für einen Sinus mit
    // Amplitude 1 dann fftSize/2 im Bin; also mit 2/fftSize skalieren.
    const float normalisation = 2.0f / static_cast<float> (fftSize);

    for (int bin = 0; bin < numBins; ++bin)
    {
        const float magnitude = fftData[static_cast<std::size_t> (bin)] * normalisation;
        const float db = magnitude > 0.0f ? juce::jmax (minimumDb, 20.0f * std::log10 (magnitude)) : minimumDb;
        auto& shown = magnitudesDb[static_cast<std::size_t> (bin)];
        shown = juce::jmax (db, shown - decayDbPerUpdate); // schnell hoch, langsam runter
    }

    return true;
}

std::span<const float> SpectrumAnalyser::getMagnitudesDb() const noexcept
{
    return magnitudesDb;
}

float SpectrumAnalyser::getFrequencyForBin (int bin) const noexcept
{
    return static_cast<float> (bin) * static_cast<float> (sampleRate.load()) / static_cast<float> (fftSize);
}

double SpectrumAnalyser::getSampleRate() const noexcept
{
    return sampleRate.load();
}

} // namespace fk::plugins::analysis
