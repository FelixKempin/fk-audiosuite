#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace fk::ui
{

/// Alle Symbole der Oberfläche. Werden als Vektorpfade gezeichnet (scharf in jeder Größe,
/// eingefärbt über Theme-Tokens). Neue Symbole: hier eintragen und in Icons.cpp zeichnen.
enum class Icon
{
    newProject,
    open,
    save,
    exportFile,
    addAudioTrack,
    addInstrumentTrack,
    addGroupTrack,
    addFxTrack,
    undo,
    redo,
    audioSettings,
    plugins,
    toolSelect,
    toolSplit,
    toolErase,
    toolDraw,
    keyEditor,
    snap,
    inspector,
    lowerZone,
    mixer,
    keyboard,
    returnToStart,
    play,
    stop,
    record,
    loop,
    metronome,
    automation,
    automationRead,
    automationWrite,
    tracksSmaller,
    tracksLarger,
    plus,
    minus,
    cut,            ///< Schere, Ausschneiden
    copy,           ///< zwei überlappende Blätter
    paste,          ///< Klemmbrett
    duplicate,      ///< Block mit Kopie dahinter
    glue,           ///< zwei Blöcke, verbunden
    trash,          ///< Papierkorb, Löschen
    octaveDown,     ///< Pfeil links mit Taste: Keyboard eine Oktave tiefer
    octaveUp,       ///< Pfeil rechts mit Taste
    openPlugin,     ///< Fenster mit Regler: Plugin-Fenster öffnen
    midiLearn,      ///< MIDI-Stecker mit Pfeil: Zuordnung lernen
    bake,           ///< Backofen mit Sichtfenster: Spur backen (ADR 0009)
    locked,         ///< Vorhängeschloss: gebacken, nicht bearbeitbar
    speaker,        ///< Lautsprecher mit Schallbögen: Vorhören
    saveAs,         ///< Diskette mit Plus: Speichern unter
    recent,         ///< Uhr: zuletzt geöffnete Projekte, Backups
    settings,       ///< drei Schieberegler: Projekteinstellungen
    eye,            ///< Auge: Spuren ein- und ausblenden
    markerAdd,      ///< Fahne mit Plus: Marker setzen
    markerPrevious, ///< Pfeil links an Fahne: zum vorigen Marker
    markerNext,     ///< Fahne mit Pfeil rechts: zum nächsten Marker
    folder,         ///< Aktenordner: Ordnerspur
    expanded,       ///< Dreieck nach unten: aufgeklappt
    collapsed,      ///< Dreieck nach rechts: zugeklappt
    rewind,         ///< zwei Dreiecke nach links: Rücklauf
    fastForward,    ///< zwei Dreiecke nach rechts: Vorlauf
    punch,          ///< Klammer mit Punkt: Punch In/Out
    preRoll,        ///< Pfeil in eine Klammer: Pre-Roll
    postRoll,       ///< Pfeil aus einer Klammer: Post-Roll
    toolMute,       ///< durchgestrichener Lautsprecher: Mute-Werkzeug
    toolZoom,       ///< Lupe: Zoom-Werkzeug
    toolGlue,       ///< Tube mit Tropfen: Kleber-Werkzeug
    nudgeLeft,      ///< kleiner Pfeil links an einer Linie: Auswahl einen Rasterschritt nach links
    nudgeRight,     ///< kleiner Pfeil rechts an einer Linie
    normalize,      ///< Welle, die bis an eine obere Linie reicht: Normalisieren
    reverse,        ///< Pfeil im Halbkreis nach links: Umkehren
    phase,          ///< Kreis mit Schrägstrich: Phase umkehren
    toolRange,      ///< gestrichelter Rahmen mit Griffen: Bereichswerkzeug
    stretch,        ///< Doppelpfeil zwischen zwei Linien: Tempo und Tonhöhe, Time-Stretch
    silence,        ///< flache Linie zwischen Wellenstücken: Stille
    gain,           ///< Welle mit Pfeil nach oben: Verstärken
    fadeIn,         ///< ansteigende Rampe
    fadeOut,        ///< abfallende Rampe
    zoomFit,        ///< Rahmen mit Pfeilen nach außen: alles zeigen
    connections,    ///< zwei Buchsen mit Kabel: Studio-Verbindungen
    history,        ///< Liste mit Pfeil zurück: Bearbeitungsverlauf
    overview,       ///< breiter Rahmen mit kleinem Fenster: Übersichtsleiste
    functions,      ///< Zauberstab: MIDI-Funktionen
    retrospective,  ///< Aufnahmepunkt mit Pfeil zurück: retrospektive Aufnahme
    star            ///< fünfzackiger Stern: Favoriten
};

/// Liefert den Pfad eines Symbols im Einheitsquadrat (0..1), Strichbreite 0,08 wo nötig.
[[nodiscard]] juce::Path makeIconPath (Icon icon);

/// Zeichnet ein Symbol in den Bereich, zentriert, mit Rand.
void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour);

/// Symbol als Drawable, z. B. für Menüeinträge (juce::PopupMenu::Item::image).
[[nodiscard]] std::unique_ptr<juce::Drawable> makeIconDrawable (Icon icon, juce::Colour colour);

} // namespace fk::ui
