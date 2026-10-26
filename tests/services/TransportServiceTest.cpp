#include "fk/services/TransportService.h"

#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::services::TransportService;
using fk::tests::TestEnvironment;

TEST_CASE ("TransportService: Vorzähler lässt sich setzen und benennen", "[services][transport]")
{
    TestEnvironment env;
    TransportService transport { env.projectService };

    REQUIRE (transport.getCountIn() == TransportService::CountIn::none); // ohne Projekt
    REQUIRE (env.projectService.createProject (env.tempFolder, "Vorzaehler").has_value());

    transport.setCountIn (TransportService::CountIn::twoBars);
    REQUIRE (transport.getCountIn() == TransportService::CountIn::twoBars);

    transport.setCountIn (TransportService::CountIn::oneBar);
    REQUIRE (transport.getCountIn() == TransportService::CountIn::oneBar);

    transport.setCountIn (TransportService::CountIn::none);
    REQUIRE (transport.getCountIn() == TransportService::CountIn::none);

    REQUIRE (TransportService::getCountInName (TransportService::CountIn::twoBars) == "2 bars");
}
