#include "fk/dsp/delay/StereoDelay.h"
#include "fk/dsp/reverb/FdnReverb.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using fk::dsp::delay::StereoDelay;
using fk::dsp::reverb::FdnReverb;

namespace
{
constexpr double sampleRate = 48000.0;

struct Stereo
{
    std::vector<float> left;
    std::vector<float> right;
};

Stereo impulseResponse (FdnReverb& reverb, double seconds)
{
    const auto count = static_cast<std::size_t> (seconds * sampleRate);
    std::vector<float> in (count, 0.0f);
    in[0] = 1.0f;
    Stereo out { std::vector<float> (count), std::vector<float> (count) };
    reverb.process (in, in, out.left, out.right);
    return out;
}

/// Energie (dB) im Fenster [from, to) Sekunden.
double energyDb (const std::vector<float>& samples, double from, double to)
{
    double sum = 0.0;

    for (auto i = static_cast<std::size_t> (from * sampleRate); i < static_cast<std::size_t> (to * sampleRate); ++i)
        sum += static_cast<double> (samples[i]) * static_cast<double> (samples[i]);

    return 10.0 * std::log10 (sum + 1.0e-30);
}
} // namespace

TEST_CASE ("FdnReverb: Nachhallzeit stimmt (Abfall 60 dB in RT60)", "[dsp][reverb]")
{
    for (const float rt60 : { 0.8f, 2.0f })
    {
        FdnReverb reverb;
        reverb.prepare (sampleRate);
        FdnReverb::Settings settings;
        settings.decaySeconds = rt60;
        settings.dampingHz = 20000.0f; // ohne Höhendämpfung: reine Breitband-Abklingzeit
        settings.modulation = 0.0f;
        settings.preDelayMs = 0.0f;
        reverb.setSettings (settings);

        const auto ir = impulseResponse (reverb, rt60 * 1.5);
        // Abfall zwischen zwei Fenstern im Abstand von rt60/2 soll ~30 dB sein.
        const auto early = energyDb (ir.left, 0.1 * rt60, 0.2 * rt60);
        const auto late = energyDb (ir.left, 0.6 * rt60, 0.7 * rt60);
        INFO ("RT60 " << rt60 << ": " << early << " dB → " << late << " dB");
        REQUIRE (early - late == Catch::Approx (30.0).margin (6.0));
    }
}

TEST_CASE ("FdnReverb: stabil bei langer Nachhallzeit, Stille bleibt Stille, Breite dekorreliert", "[dsp][reverb]")
{
    FdnReverb reverb;
    reverb.prepare (sampleRate);
    FdnReverb::Settings settings;
    settings.decaySeconds = 20.0f;
    settings.diffusion = 1.0f;
    settings.modulation = 1.0f;
    reverb.setSettings (settings);

    // 5 s volles Rauschen hinein: nichts darf explodieren oder NaN werden.
    const auto count = static_cast<std::size_t> (5.0 * sampleRate);
    std::vector<float> noise (count);
    unsigned seed = 1;

    for (auto& sample : noise)
    {
        seed = seed * 1664525u + 1013904223u;
        sample = static_cast<float> (seed >> 8) / static_cast<float> (1u << 24) * 2.0f - 1.0f;
    }

    std::vector<float> left (count), right (count);
    reverb.process (noise, noise, left, right);

    float peak = 0.0f;

    for (std::size_t i = 0; i < count; ++i)
    {
        REQUIRE (std::isfinite (left[i]));
        peak = std::max (peak, std::abs (left[i]));
    }

    REQUIRE (peak < 20.0f);

    // Breite 1: links und rechts deutlich verschieden (Korrelation klein).
    double lr = 0.0, ll = 0.0, rr = 0.0;

    for (auto i = count / 2; i < count; ++i)
    {
        lr += static_cast<double> (left[i]) * right[i];
        ll += static_cast<double> (left[i]) * left[i];
        rr += static_cast<double> (right[i]) * right[i];
    }

    REQUIRE (std::abs (lr / std::sqrt (ll * rr)) < 0.5);

    // Breite 0: Mono.
    settings.width = 0.0f;
    reverb.setSettings (settings);
    reverb.process (noise, noise, left, right);
    REQUIRE (left[count - 1] == Catch::Approx (right[count - 1]).margin (1.0e-6));

    // Nach reset() und Stille hinein bleibt alles still.
    reverb.reset();
    std::vector<float> silence (4800, 0.0f), outL (4800), outR (4800);
    reverb.process (silence, silence, outL, outR);
    REQUIRE (std::ranges::all_of (outL, [] (float s) { return s == 0.0f; }));
}

TEST_CASE ("StereoDelay: Echo nach genau der Verzögerung, Rückkopplung klingt ab, Ping-Pong wechselt die Seite",
           "[dsp][delay]")
{
    StereoDelay delay;
    delay.prepare (sampleRate);
    StereoDelay::Settings settings;
    settings.delaySeconds = 0.25f;
    settings.feedback = 0.5f;
    settings.lowCutHz = 1.0f;
    settings.highCutHz = 23000.0f;
    delay.setSettings (settings);

    const auto count = static_cast<std::size_t> (1.2 * sampleRate);
    std::vector<float> in (count, 0.0f), left (count), right (count);
    in[0] = 1.0f;
    delay.process (in, in, left, right);

    const auto at = [] (double seconds) { return static_cast<std::size_t> (seconds * sampleRate); };
    const auto peakNear = [&] (const std::vector<float>& channel, double seconds)
    {
        float peak = 0.0f;

        for (auto i = at (seconds) - 4; i <= at (seconds) + 4; ++i)
            peak = std::max (peak, std::abs (channel[i]));

        return peak;
    };

    REQUIRE (peakNear (left, 0.125) < 1.0e-4f);
    REQUIRE (peakNear (left, 0.25) > 0.8f);
    REQUIRE (peakNear (left, 0.5) == Catch::Approx (0.5f * peakNear (left, 0.25)).margin (0.05f));

    // Ping-Pong: erstes Echo links, zweites rechts.
    delay.reset();
    settings.pingPong = true;
    delay.setSettings (settings);
    delay.process (in, in, left, right);
    REQUIRE (peakNear (left, 0.25) > 0.5f);
    REQUIRE (peakNear (right, 0.25) < 1.0e-3f);
    REQUIRE (peakNear (right, 0.5) > 0.2f);
    REQUIRE (peakNear (left, 0.5) < 1.0e-3f);
}
