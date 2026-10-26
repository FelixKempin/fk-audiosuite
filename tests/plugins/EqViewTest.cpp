#include "fk/plugins/EqProcessor.h"
#include "fk/plugins/ui/BandHandles.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace fk::plugins;

TEST_CASE ("EqProcessor: Frequenzgang folgt den Parametern", "[plugins][eq]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser; // APVTS meldet Änderungen über einen Timer
    EqProcessor eq;
    eq.prepareToPlay (48000.0, 512);
    auto& parameters = eq.getParameterState();

    // Alle Bänder neutral: 0 dB überall.
    REQUIRE (eq.getMagnitudeDb (100.0f) == Catch::Approx (0.0).margin (0.05));
    REQUIRE (eq.getMagnitudeDb (5000.0f) == Catch::Approx (0.0).margin (0.05));

    // Low Mid (Glocke bei 400 Hz) um 6 dB anheben: an der Mittenfrequenz +6, weit weg kaum etwas.
    auto* gain = parameters.getParameter ("lowMidGain");
    REQUIRE (gain != nullptr);
    gain->setValueNotifyingHost (gain->convertTo0to1 (6.0f));

    REQUIRE (eq.getMagnitudeDb (400.0f) == Catch::Approx (6.0).margin (0.1));
    REQUIRE (eq.getMagnitudeDb (8000.0f) == Catch::Approx (0.0).margin (0.3));

    // Hochpass ein: unterhalb der Eckfrequenz fällt es ab.
    auto* highPass = parameters.getParameter ("hpOn");
    highPass->setValueNotifyingHost (1.0f);
    REQUIRE (eq.getMagnitudeDb (20.0f) < -20.0f);
}

TEST_CASE ("BandHandles: Griff folgt dem Parameter über die Skala", "[plugins][ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    EqProcessor eq;
    auto& parameters = eq.getParameterState();

    ui::BandHandles handles;
    handles.setBounds (0, 0, 1000, 480);
    ui::FrequencyScale scale;
    scale.bounds = { 0.0f, 0.0f, 1000.0f, 480.0f };
    handles.setScale (scale);

    handles.addHandle (*parameters.getParameter ("lowMidFreq"),
                       parameters.getParameter ("lowMidGain"),
                       parameters.getParameter ("lowMidQ"),
                       juce::Colours::white,
                       "2");
    REQUIRE (handles.getNumHandles() == 1);

    const auto before = handles.getHandleCentre (0);
    REQUIRE (before.x == Catch::Approx (scale.xForHz (400.0f)));
    REQUIRE (before.y == Catch::Approx (scale.yForDb (0.0f)));

    int changes = 0;
    handles.onChanged = [&changes] { ++changes; };

    auto* frequency = parameters.getParameter ("lowMidFreq");
    frequency->setValueNotifyingHost (frequency->convertTo0to1 (4000.0f));
    auto* gain = parameters.getParameter ("lowMidGain");
    gain->setValueNotifyingHost (gain->convertTo0to1 (-12.0f));

    const auto after = handles.getHandleCentre (0);
    REQUIRE (after.x > before.x);
    REQUIRE (after.y > before.y); // -12 dB liegt unterhalb der Nulllinie
    REQUIRE (after.x == Catch::Approx (scale.xForHz (4000.0f)).margin (0.5));
    REQUIRE (changes == 2);
}
