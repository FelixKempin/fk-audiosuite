#pragma once

#include "fk/plugins/edition/Edition.h"

#include <juce_core/juce_core.h>

#include <atomic>

namespace fk::plugins::edition
{

/// Die eine Stelle, die sagt, ob eine Funktion freigeschaltet ist. Die Oberfläche fragt
/// getEdition() und zeigt gesperrte Funktionen mit Schloss (CODING_GUIDELINES.md §10a);
/// der Audio-Thread fragt isPro() oder choose() und liest dabei nur ein Atomic.
///
/// setEdition() ruft nur der Message-Thread (die Lizenzprüfung des Produkts), die Listener werden dort benachrichtigt.
class FeatureGate final
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void editionChanged (Edition newEdition) = 0;
    };

    explicit FeatureGate (Edition initialEdition = Edition::lite);

    void setEdition (Edition newEdition);
    [[nodiscard]] Edition getEdition() const noexcept;

    /// Echtzeitsicher: ein atomares Lesen.
    [[nodiscard]] bool isPro() const noexcept;

    /// Wählt den Grenzwert der aktuellen Ausgabe, z. B. choose (2, 8) Unisono-Stimmen.
    template <typename T>
    [[nodiscard]] T choose (T liteValue, T proValue) const noexcept
    {
        return isPro() ? proValue : liteValue;
    }

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

private:
    std::atomic<Edition> edition;
    juce::ListenerList<Listener> listeners;
};

} // namespace fk::plugins::edition
