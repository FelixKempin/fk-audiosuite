#include "fk/plugins/edition/FeatureGate.h"

namespace fk::plugins::edition
{

FeatureGate::FeatureGate (Edition initialEdition)
    : edition (initialEdition)
{
}

void FeatureGate::setEdition (Edition newEdition)
{
    if (edition.exchange (newEdition) == newEdition)
        return;

    listeners.call ([newEdition] (Listener& listener) { listener.editionChanged (newEdition); });
}

Edition FeatureGate::getEdition() const noexcept
{
    return edition.load (std::memory_order_relaxed);
}

bool FeatureGate::isPro() const noexcept
{
    return getEdition() == Edition::pro;
}

void FeatureGate::addListener (Listener* listener)
{
    listeners.add (listener);
}

void FeatureGate::removeListener (Listener* listener)
{
    listeners.remove (listener);
}

} // namespace fk::plugins::edition
