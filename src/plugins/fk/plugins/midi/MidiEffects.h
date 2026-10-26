#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <optional>
#include <vector>

namespace fk::plugins::midi
{

/// Transponiert Noten und verschiebt den Anschlag. Merkt sich je gespielter Note, welche Note
/// daraus wurde, damit das Note-Off auch dann passt, wenn der Regler zwischendurch bewegt wurde.
class Transposer final
{
public:
    void process (juce::MidiBuffer& midi, int semitones, int velocityOffset);
    void reset();

private:
    std::array<int, 128 * 16> sounding {}; ///< [Kanal * 128 + Note] → ausgelöste Note + 1 (0 = keine)
};

/// Akkord aus einer Taste: legt Intervalle über die gespielte Note.
class ChordGenerator final
{
public:
    enum class Kind
    {
        major,
        minor,
        sus4,
        major7,
        minor7,
        dominant7,
        power,
        octave
    };

    [[nodiscard]] static std::vector<int> intervals (Kind kind);
    [[nodiscard]] static juce::StringArray kindNames();

    void process (juce::MidiBuffer& midi, Kind kind);
    void reset();

private:
    std::array<std::vector<int>, 128 * 16> sounding; ///< je gespielter Note die ausgelösten Noten
};

/// Arpeggiator: gehaltene Noten werden nacheinander im Raster gespielt. Läuft die Wiedergabe, folgt er
/// der Songposition (ppq) des Hosts; sonst einer eigenen Uhr im Projekttempo.
class Arpeggiator final
{
public:
    enum class Mode
    {
        up,
        down,
        upDown,
        random,
        asPlayed
    };

    struct Settings
    {
        double rateBeats = 0.25; ///< Viertel = 1, Sechzehntel = 0,25
        Mode mode = Mode::up;
        int octaves = 1;
        double gate = 0.5; ///< Anteil des Schritts, den die Note klingt
    };

    [[nodiscard]] static juce::StringArray modeNames();

    void prepare (double sampleRate);
    void reset();

    /// Ersetzt die Noten in midi durch das Arpeggio. hostPpq: Songposition am Blockanfang, wenn die
    /// Wiedergabe läuft; sonst leer (eigene Uhr). Andere Ereignisse (CC, Pitch-Bend) gehen durch.
    void process (juce::MidiBuffer& midi, int numSamples, double bpm, std::optional<double> hostPpq, const Settings& settings);

    /// Die Reihenfolge, in der die gehaltenen Noten gespielt werden (für Tests sichtbar).
    [[nodiscard]] std::vector<int> pattern (const Settings& settings) const;

private:
    struct Held
    {
        int note;
        int velocity;
    };

    std::vector<Held> held; ///< in Anschlagreihenfolge
    double sampleRate = 48000.0;
    double ownPpq = 0.0;
    long long lastStep = -1;
    std::size_t patternIndex = 0;
    int soundingNote = -1;
    int soundingChannel = 1;
    int lastChannel = 1;
    int samplesUntilOff = 0;
    juce::Random random;
};

} // namespace fk::plugins::midi
