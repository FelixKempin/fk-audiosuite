#include "fk/plugins/edition/FeatureGate.h"

#include <catch2/catch_test_macros.hpp>

using namespace fk::plugins::edition;

namespace
{
struct CountingListener final : FeatureGate::Listener
{
    int calls = 0;
    Edition last = Edition::lite;
    void editionChanged (Edition newEdition) override
    {
        ++calls;
        last = newEdition;
    }
};
} // namespace

TEST_CASE ("FeatureGate: wählt Grenzwerte nach Ausgabe und meldet Wechsel genau einmal", "[plugins][edition]")
{
    FeatureGate gate;
    CountingListener listener;
    gate.addListener (&listener);

    REQUIRE_FALSE (gate.isPro());
    REQUIRE (gate.choose (2, 8) == 2);

    gate.setEdition (Edition::pro);
    gate.setEdition (Edition::pro); // kein zweiter Aufruf

    REQUIRE (gate.isPro());
    REQUIRE (gate.choose (2, 8) == 8);
    REQUIRE (listener.calls == 1);
    REQUIRE (listener.last == Edition::pro);

    gate.removeListener (&listener);
}
