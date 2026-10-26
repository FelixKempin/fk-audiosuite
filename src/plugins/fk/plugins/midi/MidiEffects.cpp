#include "fk/plugins/midi/MidiEffects.h"

#include <algorithm>
#include <cmath>

namespace fk::plugins::midi
{

namespace
{
std::size_t slot (const juce::MidiMessage& message)
{
    return static_cast<std::size_t> ((message.getChannel() - 1) * 128 + message.getNoteNumber());
}
} // namespace

// ---------------------------------------------------------------------------------------------
// Transposer

void Transposer::reset()
{
    sounding.fill (0);
}

void Transposer::process (juce::MidiBuffer& midi, int semitones, int velocityOffset)
{
    juce::MidiBuffer out;

    for (const auto metadata : midi)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            const int note = message.getNoteNumber() + semitones;

            if (note < 0 || note > 127)
                continue; // außerhalb der Tastatur: weglassen, auch das Note-Off später

            const auto velocity = static_cast<juce::uint8> (std::clamp (message.getVelocity() + velocityOffset, 1, 127));
            sounding[slot (message)] = note + 1;
            out.addEvent (juce::MidiMessage::noteOn (message.getChannel(), note, velocity), metadata.samplePosition);
        }
        else if (message.isNoteOff())
        {
            if (const int stored = sounding[slot (message)]; stored > 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (message.getChannel(), stored - 1), metadata.samplePosition);
                sounding[slot (message)] = 0;
            }
        }
        else
        {
            out.addEvent (message, metadata.samplePosition);
        }
    }

    midi.swapWith (out);
}

// ---------------------------------------------------------------------------------------------
// ChordGenerator

std::vector<int> ChordGenerator::intervals (Kind kind)
{
    switch (kind)
    {
        case Kind::major:
            return { 0, 4, 7 };
        case Kind::minor:
            return { 0, 3, 7 };
        case Kind::sus4:
            return { 0, 5, 7 };
        case Kind::major7:
            return { 0, 4, 7, 11 };
        case Kind::minor7:
            return { 0, 3, 7, 10 };
        case Kind::dominant7:
            return { 0, 4, 7, 10 };
        case Kind::power:
            return { 0, 7, 12 };
        case Kind::octave:
            return { 0, 12 };
    }

    return { 0 };
}

juce::StringArray ChordGenerator::kindNames()
{
    return { TRANS ("Major"), TRANS ("Minor"), TRANS ("Sus4"), TRANS ("Maj7"), TRANS ("Min7"), TRANS ("Dom7"), TRANS ("Power"), TRANS ("Octave") };
}

void ChordGenerator::reset()
{
    for (auto& notes : sounding)
        notes.clear();
}

void ChordGenerator::process (juce::MidiBuffer& midi, Kind kind)
{
    juce::MidiBuffer out;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            auto& notes = sounding[slot (message)];
            notes.clear();

            for (const int interval : intervals (kind))
                if (const int note = message.getNoteNumber() + interval; note <= 127)
                {
                    notes.push_back (note);
                    out.addEvent (juce::MidiMessage::noteOn (message.getChannel(), note, message.getVelocity()),
                                  metadata.samplePosition);
                }
        }
        else if (message.isNoteOff())
        {
            auto& notes = sounding[slot (message)];

            for (const int note : notes)
                out.addEvent (juce::MidiMessage::noteOff (message.getChannel(), note), metadata.samplePosition);

            notes.clear();
        }
        else
        {
            out.addEvent (message, metadata.samplePosition);
        }
    }

    midi.swapWith (out);
}

// ---------------------------------------------------------------------------------------------
// Arpeggiator

juce::StringArray Arpeggiator::modeNames()
{
    return { TRANS ("Up"), TRANS ("Down"), TRANS ("Up and down"), TRANS ("Random"), TRANS ("As played") };
}

void Arpeggiator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void Arpeggiator::reset()
{
    held.clear();
    ownPpq = 0.0;
    lastStep = -1;
    patternIndex = 0;
    soundingNote = -1;
    samplesUntilOff = 0;
}

std::vector<int> Arpeggiator::pattern (const Settings& settings) const
{
    std::vector<int> base;

    for (const auto& note : held)
        base.push_back (note.note);

    if (settings.mode != Mode::asPlayed)
        std::ranges::sort (base);

    std::vector<int> notes;

    for (int octave = 0; octave < std::max (1, settings.octaves); ++octave)
        for (const int note : base)
            if (note + octave * 12 <= 127)
                notes.push_back (note + octave * 12);

    switch (settings.mode)
    {
        case Mode::down:
            std::ranges::reverse (notes);
            break;
        case Mode::upDown:
            // Hoch und wieder runter, ohne die Spitze und den Boden doppelt zu spielen.
            for (auto i = static_cast<int> (notes.size()) - 2; i > 0; --i)
                notes.push_back (notes[static_cast<std::size_t> (i)]);
            break;
        case Mode::up:
        case Mode::random:
        case Mode::asPlayed:
            break;
    }

    return notes;
}

void Arpeggiator::process (juce::MidiBuffer& midi,
                           int numSamples,
                           double bpm,
                           std::optional<double> hostPpq,
                           const Settings& settings)
{
    const double rate = std::max (settings.rateBeats, 1.0 / 32.0);
    const double ppqPerSample = std::max (bpm, 1.0) / 60.0 / sampleRate;
    const double stepSamples = rate / ppqPerSample;
    double ppq = hostPpq.value_or (ownPpq);

    juce::MidiBuffer out;
    auto incoming = midi.cbegin();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Eingehende Ereignisse an ihrer Stelle: Tasten verändern die gehaltenen Noten, der Rest geht durch.
        while (incoming != midi.cend() && (*incoming).samplePosition <= sample)
        {
            const auto message = (*incoming).getMessage();

            if (message.isNoteOn())
            {
                held.push_back ({ message.getNoteNumber(), message.getVelocity() });
                lastChannel = message.getChannel();
            }
            else if (message.isNoteOff())
            {
                std::erase_if (held, [&message] (const Held& note) { return note.note == message.getNoteNumber(); });
            }
            else
            {
                out.addEvent (message, sample);
            }

            ++incoming;
        }

        if (soundingNote >= 0 && --samplesUntilOff <= 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (soundingChannel, soundingNote), sample);
            soundingNote = -1;
        }

        const auto step = static_cast<long long> (std::floor (ppq / rate + 1.0e-9));

        if (step != lastStep)
        {
            lastStep = step;

            if (soundingNote >= 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (soundingChannel, soundingNote), sample);
                soundingNote = -1;
            }

            const auto notes = pattern (settings);

            if (notes.empty())
            {
                patternIndex = 0;
            }
            else
            {
                const auto index = settings.mode == Mode::random
                                       ? static_cast<std::size_t> (random.nextInt (static_cast<int> (notes.size())))
                                       : patternIndex++ % notes.size();
                const int note = notes[index];

                // Anschlag der gehaltenen Taste, deren Oktavlage gerade spielt.
                int velocity = held.front().velocity;

                for (const auto& key : held)
                    if (note >= key.note && (note - key.note) % 12 == 0)
                        velocity = key.velocity;

                out.addEvent (juce::MidiMessage::noteOn (lastChannel, note, static_cast<juce::uint8> (velocity)), sample);
                soundingNote = note;
                soundingChannel = lastChannel;
                samplesUntilOff = std::max (1, juce::roundToInt (stepSamples * std::clamp (settings.gate, 0.05, 1.0)));
            }
        }

        ppq += ppqPerSample;
    }

    ownPpq = ppq;
    midi.swapWith (out);
}

} // namespace fk::plugins::midi
