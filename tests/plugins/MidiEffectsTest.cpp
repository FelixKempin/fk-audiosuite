#include "fk/plugins/midi/MidiEffects.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using fk::plugins::midi::Arpeggiator;
using fk::plugins::midi::ChordGenerator;
using fk::plugins::midi::Transposer;

namespace
{
struct Event
{
    int sample;
    bool on;
    int note;
    int velocity;
};

std::vector<Event> notesOf (const juce::MidiBuffer& midi, int offset = 0)
{
    std::vector<Event> events;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOnOrOff())
            events.push_back ({ metadata.samplePosition + offset, message.isNoteOn(), message.getNoteNumber(), message.getVelocity() });
    }

    return events;
}
} // namespace

TEST_CASE ("MIDI-Effekte: Transpose und Akkord", "[plugins][midi][b3]")
{
    Transposer transposer;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 120, static_cast<juce::uint8> (100)), 0); // +12 → außerhalb
    transposer.process (midi, 12, -20);
    auto events = notesOf (midi);
    REQUIRE (events.size() == 1);
    REQUIRE (events[0].note == 72);
    REQUIRE (events[0].velocity == 80);

    // Regler bewegt, während die Taste gehalten wird: Das Note-Off trifft trotzdem die klingende Note.
    juce::MidiBuffer release;
    release.addEvent (juce::MidiMessage::noteOff (1, 60), 10);
    release.addEvent (juce::MidiMessage::noteOff (1, 120), 10);
    release.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 11);
    transposer.process (release, 5, 0);
    events = notesOf (release);
    REQUIRE (events.size() == 1);
    REQUIRE (events[0].note == 72);
    REQUIRE (release.getNumEvents() == 2); // Sustain geht durch

    ChordGenerator chord;
    juce::MidiBuffer chordMidi;
    chordMidi.addEvent (juce::MidiMessage::noteOn (2, 60, static_cast<juce::uint8> (90)), 5);
    chord.process (chordMidi, ChordGenerator::Kind::minor7);
    events = notesOf (chordMidi);
    REQUIRE (events.size() == 4);
    REQUIRE (events[1].note == 63);
    REQUIRE (events[3].note == 70);

    juce::MidiBuffer chordOff;
    chordOff.addEvent (juce::MidiMessage::noteOff (2, 60), 0);
    chord.process (chordOff, ChordGenerator::Kind::major); // anderer Akkord, gleiche Note-Offs
    events = notesOf (chordOff);
    REQUIRE (events.size() == 4);
    REQUIRE (events[1].note == 63);
}

TEST_CASE ("MIDI-Effekte: Arpeggiator im Raster", "[plugins][midi][b3]")
{
    Arpeggiator arp;
    arp.prepare (48000.0);
    Arpeggiator::Settings settings; // 1/16, aufwärts, 1 Oktave, Gate 50 %
    constexpr int step = 6000;      // 1/16 bei 120 BPM = 0,125 s

    // C, G, E gedrückt (nicht sortiert); vier Schritte.
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, static_cast<juce::uint8> (90)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, static_cast<juce::uint8> (80)), 0);

    std::vector<Event> all;

    for (int block = 0; block < 4; ++block)
    {
        if (block > 0)
            midi.clear();

        arp.process (midi, step, 120.0, std::nullopt, settings);

        for (const auto& event : notesOf (midi, block * step))
            all.push_back (event);
    }

    std::vector<int> played;
    for (const auto& event : all)
        if (event.on)
            played.push_back (event.note);

    REQUIRE (played == std::vector<int> { 60, 64, 67, 60 });
    REQUIRE (all[0].sample == 0);
    REQUIRE (all[1].on == false);
    REQUIRE (all[1].sample == step / 2); // Gate 50 %
    REQUIRE (all[2].sample == step);     // nächster Schritt
    REQUIRE (all[2].velocity == 80);     // E wurde mit 80 angeschlagen

    // Muster der anderen Richtungen und Oktaven.
    settings.mode = Arpeggiator::Mode::down;
    REQUIRE (arp.pattern (settings) == std::vector<int> { 67, 64, 60 });
    settings.mode = Arpeggiator::Mode::upDown;
    REQUIRE (arp.pattern (settings) == std::vector<int> { 60, 64, 67, 64 });
    settings.mode = Arpeggiator::Mode::asPlayed;
    REQUIRE (arp.pattern (settings) == std::vector<int> { 60, 67, 64 });
    settings.mode = Arpeggiator::Mode::up;
    settings.octaves = 2;
    REQUIRE (arp.pattern (settings) == std::vector<int> { 60, 64, 67, 72, 76, 79 });

    // Loslassen: keine neuen Noten mehr.
    juce::MidiBuffer released;
    for (const int note : { 60, 64, 67 })
        released.addEvent (juce::MidiMessage::noteOff (1, note), 0);
    arp.process (released, step * 2, 120.0, std::nullopt, settings);
    for (const auto& event : notesOf (released))
        REQUIRE_FALSE (event.on);
}
