#pragma once

#include "fk/plugins/ui/PanelDescription.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <optional>

namespace fk::plugins
{

/// Name des "Formats", unter dem unsere Plugins im Host erscheinen.
inline constexpr auto internalFormatName = "FK";

/// Gemeinsame Basis aller eigenen Plugins. Erbt von juce::AudioPluginInstance (nicht nur
/// AudioProcessor), damit der Host sie wie fremde Plugins behandelt: dieselbe Liste,
/// dieselbe Automation, derselbe Zustand im Projekt.
///
/// Parameter laufen über juce::AudioProcessorValueTreeState (APVTS): Der Host liest und
/// schreibt sie thread-sicher, wir lesen im Audio-Thread nur std::atomic<float>*.
///
/// Echtzeit: processBlock() der Unterklassen läuft im Audio-Thread (CODING_GUIDELINES.md §8).
///
/// Hinweis für neue Plugins: Wer processBlock (AudioBuffer<float>&, ...) überschreibt, verdeckt
/// die double-Variante der Basisklasse. Deshalb steht in jeder Unterklasse
/// `using InternalPlugin::processBlock;` (sonst warnt -Woverloaded-virtual, bei uns ein Fehler).
class InternalPlugin : public juce::AudioPluginInstance
{
public:
    /// Beschreibung eines Plugins für Liste und Wiedererkennung im Projekt.
    struct Info
    {
        juce::String name;
        juce::String category;     ///< z. B. "Dynamik", "EQ", "Verzerrung", "Instrument"
        int uniqueId = 0;          ///< stabil halten: steht in gespeicherten Projekten
        bool isInstrument = false; ///< Instrument: nimmt MIDI an, hat keinen Audio-Eingang
        bool hasSidechain = false; ///< zweiter Stereo-Eingang "Sidechain" (Compressor, Gate)
        int outputPairs = 1;       ///< Stereo-Ausgänge; ab 2 Multi-Out (im Rack einzeln in den Mixer)
        bool isMidiEffect = false; ///< verändert MIDI vor dem Instrument (Arpeggiator, Akkord); Audio läuft durch
    };

    InternalPlugin (Info info, juce::AudioProcessorValueTreeState::ParameterLayout parameterLayout);

    [[nodiscard]] static juce::PluginDescription makeDescription (const Info& info);

    // juce::AudioPluginInstance
    void fillInPluginDescription (juce::PluginDescription& description) const override;

    // juce::AudioProcessor – gleiches Verhalten für alle unsere Effekte
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;
    bool hasEditor() const override;

    /// Baut die Oberfläche aus describePanel() mit dem UI-Kit (ui::PanelEditor).
    juce::AudioProcessorEditor* createEditor() override;

    /// Die Oberfläche als Daten: Abschnitte mit Parameter-IDs und Anzeigen. Standard: alle
    /// Parameter in Reihen zu sechs. Plugins überschreiben das, um sinnvoll zu gruppieren.
    [[nodiscard]] virtual ui::PanelDescription describePanel();

    /// Parameterzustand für Oberflächen, die über PanelEditor hinausgehen (Szenen, Produkte).
    [[nodiscard]] juce::AudioProcessorValueTreeState& getParameterState() noexcept;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

protected:
    /// Effekte: Stereo rein, Stereo raus. Instrumente: nur Stereo raus.
    /// Geschützt, weil JUCE BusesProperties nur Unterklassen zeigt.
    [[nodiscard]] static BusesProperties busesFor (const Info& info);

    /// Die Sidechain-Kanäle im Block, falls das Plugin einen Sidechain-Eingang hat und er Kanäle trägt.
    /// Tracktion legt sie hinter die Hauptkanäle (2 und 3); ohne gewählte Quelle sind sie still.
    [[nodiscard]] std::optional<juce::AudioBuffer<float>> sidechainOf (juce::AudioBuffer<float>& buffer);

    /// Liest einen Parameter als Atomic, den processBlock() ohne Lock abfragen darf.
    [[nodiscard]] std::atomic<float>& parameter (const juce::String& parameterId);

    juce::AudioProcessorValueTreeState parameters;

private:
    Info pluginInfo;
};

} // namespace fk::plugins
