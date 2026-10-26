#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <compare>
#include <optional>
#include <vector>

namespace fk::engine
{

namespace te = tracktion;

/// Wie eine Spielweise beim Abspielen beim Plugin ankommt (ORCHESTRA.md §5a.3): auf einem eigenen
/// Kanal, per Keyswitch-Note vor der Note und/oder per CC-Wert vor der Note.
struct ArticulationOutput
{
    int channel = 0;    ///< 1..16, 0 = Kanal des Clips
    int keyswitch = -1; ///< Note 0..127, -1 = keiner
    int cc = -1;        ///< Controller 0..127, -1 = keiner
    int ccValue = 0;

    auto operator<=> (const ArticulationOutput&) const = default; // Schlüssel beim Gruppieren der Noten
};

/// Eine Spielweise: feste Kennung ("stacc"), Anzeigename, kurzes Zeichen für Key Editor und Notenbild.
struct Articulation
{
    juce::String id;
    juce::String name;
    juce::String symbol;
    ArticulationOutput output;
};

/// Expression Map (Cubase: Expression Map): die Spielweisen eines Instruments und ihre Übertragung.
/// Liegt als JSON in der Spur (eine Kopie je Projekt, wie in Cubase), damit Projekt und Wiedergabe
/// ohne die Bibliothek der Maps auskommen.
struct ExpressionMap
{
    juce::String id;
    juce::String name;
    std::vector<Articulation> articulations;

    [[nodiscard]] const Articulation* find (const juce::String& articulationId) const;

    [[nodiscard]] juce::String toJson() const;
    [[nodiscard]] static std::optional<ExpressionMap> fromJson (const juce::String& json);
};

/// Eigenschaften im Tracktion-Zustand: Map an der Spur, Spielweise an der Note.
inline const juce::Identifier expressionMapProperty ("fkExpressionMap");
inline const juce::Identifier noteArticulationProperty ("fkArt");

/// Erzeugt die Wiedergabefolge eines MIDI-Clips mit Spielweisen: Noten je Spielweise auf ihrem Kanal,
/// davor Keyswitch und CC. Jede Note bringt ihren Zustand selbst mit; Springen im Song braucht darum
/// kein Nachsuchen. Ohne Map oder ohne Spielweisen genau die Standardfolge von Tracktion.
[[nodiscard]] juce::MidiMessageSequence createSequenceWithArticulations (const te::MidiList& list,
                                                                         te::MidiClip& clip,
                                                                         te::MidiList::TimeBase timeBase,
                                                                         bool generateMPE);

} // namespace fk::engine
