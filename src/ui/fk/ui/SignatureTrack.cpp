#include "fk/ui/SignatureTrack.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr float pointRadius = 4.0f;
constexpr int markerHitWidth = 60;

const std::vector<services::TimeSignature>& commonSignatures()
{
    static const std::vector<services::TimeSignature> signatures { { 4, 4 }, { 3, 4 }, { 2, 4 },  { 6, 8 },
                                                                   { 5, 4 }, { 7, 8 }, { 12, 8 }, { 9, 8 } };
    return signatures;
}

/// Inhalt der Sprechblase zur Tonartwahl: Grundton und Skala, Änderung wirkt sofort.
class KeyChooser final : public juce::Component
{
public:
    KeyChooser (int rootNote, services::ScaleKind scale, std::function<void (int, services::ScaleKind)> onChanged)
        : apply (std::move (onChanged))
    {
        for (int note = 0; note < 12; ++note)
            rootBox.addItem (services::SignatureService::getNoteName (note), note + 1);

        for (int i = 0; i <= static_cast<int> (services::ScaleKind::melodicMinor); ++i)
            scaleBox.addItem (services::SignatureService::getScaleName (static_cast<services::ScaleKind> (i)), i + 1);

        rootBox.setSelectedId (rootNote + 1, juce::dontSendNotification);
        scaleBox.setSelectedId (static_cast<int> (scale) + 1, juce::dontSendNotification);
        rootBox.setTooltip (TRANS ("Root note"));
        scaleBox.setTooltip (TRANS ("Scale"));
        rootBox.onChange = [this] { changed(); };
        scaleBox.onChange = [this] { changed(); };
        addAndMakeVisible (rootBox);
        addAndMakeVisible (scaleBox);
        setSize (260, 32);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (theme::spacingSmall);
        rootBox.setBounds (area.removeFromLeft (70));
        area.removeFromLeft (theme::spacingSmall);
        scaleBox.setBounds (area);
    }

private:
    void changed()
    {
        if (apply)
            apply (rootBox.getSelectedId() - 1, static_cast<services::ScaleKind> (scaleBox.getSelectedId() - 1));
    }

    std::function<void (int, services::ScaleKind)> apply;
    juce::ComboBox rootBox;
    juce::ComboBox scaleBox;
};
} // namespace

SignatureTrack::SignatureTrack (services::SignatureService& signatureService,
                                services::TimelineService& timelineService)
    : signature (signatureService)
    , timeline (timelineService)
{
    setTooltip (
        TRANS ("Signature track: click sets tempo, time signature or key on the grid. Tempo point: drag (time/BPM), double-click jump/ramp, right-click removes"));
    signature.addListener (*this);
    refresh();
}

SignatureTrack::~SignatureTrack()
{
    signature.removeListener (*this);
}

void SignatureTrack::setSettings (LaneSettings newSettings)
{
    settings = newSettings;
    repaint();
}

void SignatureTrack::signatureChanged()
{
    refresh();
}

void SignatureTrack::refresh()
{
    tempoPoints = signature.getTempoPoints();
    timeSignaturePoints = signature.getTimeSignaturePoints();
    keyPoints = signature.getKeyPoints();

    // BPM-Skala der Tempolage: alle Punkte sichtbar, mit Rand, mindestens 40..200.
    minBpm = 40.0;
    maxBpm = 200.0;

    for (const auto& point : tempoPoints)
    {
        minBpm = juce::jmin (minBpm, point.bpm - 20.0);
        maxBpm = juce::jmax (maxBpm, point.bpm + 20.0);
    }

    repaint();
}

SignatureTrack::Lane SignatureTrack::laneAt (int y) const
{
    return static_cast<Lane> (juce::jlimit (0, numLanes - 1, y / laneHeight));
}

juce::Rectangle<int> SignatureTrack::laneArea (Lane lane) const
{
    return {
        theme::trackHeaderWidth, static_cast<int> (lane) * laneHeight, getWidth() - theme::trackHeaderWidth, laneHeight
    };
}

int SignatureTrack::xForSeconds (double seconds) const
{
    return theme::trackHeaderWidth + settings.mapping.secondsToX (seconds);
}

double SignatureTrack::secondsForX (int x, bool snap) const
{
    const auto seconds = juce::jmax (0.0, settings.mapping.xToSeconds (x - theme::trackHeaderWidth));
    return snap && settings.snapEnabled ? timeline.snapSeconds (seconds, settings.gridResolution) : seconds;
}

float SignatureTrack::yForBpm (double bpm) const
{
    const auto area = laneArea (Lane::tempo).toFloat().reduced (0.0f, pointRadius);
    const auto proportion = (bpm - minBpm) / (maxBpm - minBpm);
    return area.getBottom() - static_cast<float> (proportion) * area.getHeight();
}

double SignatureTrack::bpmForY (float y) const
{
    const auto area = laneArea (Lane::tempo).toFloat().reduced (0.0f, pointRadius);
    const auto proportion = juce::jlimit (0.0f, 1.0f, (area.getBottom() - y) / area.getHeight());
    return minBpm + proportion * (maxBpm - minBpm);
}

int SignatureTrack::findTempoPointAt (juce::Point<int> position) const
{
    for (int i = static_cast<int> (tempoPoints.size()) - 1; i >= 0; --i)
    {
        const auto& point = tempoPoints[static_cast<std::size_t> (i)];
        const juce::Point<float> centre (static_cast<float> (xForSeconds (point.seconds)), yForBpm (point.bpm));

        if (centre.getDistanceFrom (position.toFloat()) <= pointRadius * 2.0f)
            return i;
    }

    return -1;
}

int SignatureTrack::findMarkerAt (Lane lane, int x) const
{
    if (lane == Lane::timeSignature)
    {
        for (int i = static_cast<int> (timeSignaturePoints.size()) - 1; i >= 0; --i)
        {
            const int markerX = xForSeconds (timeSignaturePoints[static_cast<std::size_t> (i)].seconds);

            if (x >= markerX && x < markerX + markerHitWidth)
                return i;
        }
    }
    else if (lane == Lane::key)
    {
        for (int i = static_cast<int> (keyPoints.size()) - 1; i >= 0; --i)
        {
            const int markerX = xForSeconds (keyPoints[static_cast<std::size_t> (i)].seconds);

            if (x >= markerX && x < markerX + markerHitWidth)
                return i;
        }
    }

    return -1;
}

void SignatureTrack::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);

    // Lagennamen links, Lagenhintergrund rechts.
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    const char* const names[numLanes] = { NEEDS_TRANS ("Tempo"), NEEDS_TRANS ("Time signature"), NEEDS_TRANS ("Key") };

    for (int i = 0; i < numLanes; ++i)
    {
        const auto lane = static_cast<Lane> (i);
        g.setColour (i % 2 == 0 ? theme::backgroundInput : theme::backgroundBase);
        g.fillRect (laneArea (lane));
        g.setColour (theme::textSecondary);
        g.drawText (
            juce::translate (names[i]),
            juce::Rectangle<int> (
                theme::spacingMedium + theme::spacingSmall, i * laneHeight, theme::trackHeaderWidth, laneHeight),
            juce::Justification::centredLeft);
    }

    paintTempoLane (g);
    paintMarkerLane (g, Lane::timeSignature);
    paintMarkerLane (g, Lane::key);

    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
    g.drawVerticalLine (theme::trackHeaderWidth - 1, 0.0f, static_cast<float> (getHeight()));
}

void SignatureTrack::paintTempoLane (juce::Graphics& g)
{
    const auto area = laneArea (Lane::tempo);

    if (tempoPoints.empty())
        return;

    // Verlauf aus dem Service je Pixelspalte: Rampen erscheinen als Linie, Sprünge als Stufe.
    juce::Path curve;

    for (int x = area.getX(); x < area.getRight(); ++x)
    {
        const auto y = yForBpm (signature.getTempoAt (secondsForX (x, false)));

        if (x == area.getX())
            curve.startNewSubPath (static_cast<float> (x), y);
        else
            curve.lineTo (static_cast<float> (x), y);
    }

    g.setColour (theme::solo.withAlpha (0.9f));
    g.strokePath (curve, juce::PathStrokeType (1.5f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    for (std::size_t i = 0; i < tempoPoints.size(); ++i)
    {
        auto point = tempoPoints[i];

        if (tempoDrag && tempoDrag->index == static_cast<int> (i))
        {
            point.bpm = tempoDrag->bpm;
            point.seconds = tempoDrag->seconds;
        }

        const auto centre = juce::Point<float> (static_cast<float> (xForSeconds (point.seconds)), yForBpm (point.bpm));

        if (centre.x < static_cast<float> (area.getX()) - pointRadius)
            continue;

        g.setColour (point.ramp ? theme::accent : theme::solo);
        g.fillEllipse (juce::Rectangle<float> (pointRadius * 2.0f, pointRadius * 2.0f).withCentre (centre));
        g.setColour (theme::textPrimary);
        g.drawText (juce::String (point.bpm, 1) + (point.ramp ? " /" : ""),
                    juce::Rectangle<float> (centre.x + pointRadius + 2.0f,
                                            static_cast<float> (area.getY()),
                                            60.0f,
                                            static_cast<float> (laneHeight)),
                    juce::Justification::centredLeft);
    }
}

void SignatureTrack::paintMarkerLane (juce::Graphics& g, Lane lane)
{
    const auto area = laneArea (lane);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));

    auto drawMarker = [&] (double seconds, const juce::String& text)
    {
        const int x = xForSeconds (seconds);

        if (x < area.getX() - markerHitWidth || x > area.getRight())
            return;

        g.setColour (lane == Lane::key ? theme::meterLow : theme::accent);
        g.fillRect (x, area.getY() + 2, 2, laneHeight - 4);
        g.setColour (theme::textPrimary);
        g.drawText (text, x + 5, area.getY(), markerHitWidth, laneHeight, juce::Justification::centredLeft);
    };

    if (lane == Lane::timeSignature)
        for (const auto& point : timeSignaturePoints)
            drawMarker (point.seconds,
                        juce::String (point.signature.numerator) + "/" + juce::String (point.signature.denominator));
    else
        for (const auto& point : keyPoints)
            drawMarker (point.seconds, point.name);
}

void SignatureTrack::mouseDown (const juce::MouseEvent& event)
{
    if (event.x < theme::trackHeaderWidth)
        return;

    const auto lane = laneAt (event.y);

    if (lane == Lane::tempo)
    {
        const int hit = findTempoPointAt (event.getPosition());

        if (event.mods.isPopupMenu())
        {
            if (hit > 0)
                if (const auto result = signature.removeTempoPoint (hit); ! result)
                    reportError (result.error());

            return;
        }

        if (hit >= 0)
        {
            const auto& point = tempoPoints[static_cast<std::size_t> (hit)];
            tempoDrag = TempoDrag { hit, point.bpm, point.seconds, false };
            return;
        }

        const auto seconds = secondsForX (event.x, true);
        const auto inserted = signature.insertTempoPoint (seconds, signature.getTempoAt (seconds), false);

        if (! inserted)
            reportError (inserted.error());

        return;
    }

    const int hit = findMarkerAt (lane, event.x);
    const auto seconds = secondsForX (event.x, true);

    if (event.mods.isPopupMenu())
    {
        if (hit > 0)
        {
            const auto result =
                lane == Lane::timeSignature ? signature.removeTimeSignature (hit) : signature.removeKey (hit);

            if (! result)
                reportError (result.error());
        }

        return;
    }

    if (lane == Lane::timeSignature)
        showTimeSignatureMenu (event.getScreenPosition(), seconds, hit);
    else
        showKeyChooser (juce::Rectangle<int> (event.getScreenX(), event.getScreenY(), 1, 1), seconds, hit);
}

void SignatureTrack::mouseDrag (const juce::MouseEvent& event)
{
    if (! tempoDrag)
        return;

    // Vorschau: Das Modell ändert sich erst beim Loslassen (ein Undo-Schritt, keine Umbauten).
    tempoDrag->bpm = bpmForY (event.position.y);

    if (tempoDrag->index > 0)
        tempoDrag->seconds = secondsForX (event.x, true);

    tempoDrag->moved = true;
    repaint();
}

void SignatureTrack::mouseUp (const juce::MouseEvent&)
{
    if (! tempoDrag)
        return;

    const auto drag = *tempoDrag;
    tempoDrag.reset();

    if (! drag.moved || drag.index < 0 || drag.index >= static_cast<int> (tempoPoints.size()))
        return;

    const auto& point = tempoPoints[static_cast<std::size_t> (drag.index)];

    if (const auto result = signature.setTempoPoint (drag.index, drag.bpm, point.ramp); ! result)
        reportError (result.error());

    if (drag.index > 0 && std::abs (drag.seconds - point.seconds) > 1.0e-6)
        if (const auto result = signature.moveTempoPoint (drag.index, drag.seconds); ! result)
            reportError (result.error());
}

void SignatureTrack::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (laneAt (event.y) != Lane::tempo)
        return;

    const int hit = findTempoPointAt (event.getPosition());

    if (hit < 0)
        return;

    tempoDrag.reset();
    const auto& point = tempoPoints[static_cast<std::size_t> (hit)];

    if (const auto result = signature.setTempoPoint (hit, point.bpm, ! point.ramp); ! result)
        reportError (result.error());
}

void SignatureTrack::showTimeSignatureMenu (juce::Point<int> screenPosition, double seconds, int existingIndex)
{
    juce::PopupMenu menu;
    int id = 1;

    for (const auto& candidate : commonSignatures())
        menu.addItem (id++, juce::String (candidate.numerator) + "/" + juce::String (candidate.denominator));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
                        [this, seconds, existingIndex] (int choice)
                        {
                            if (choice <= 0)
                                return;

                            const auto chosen = commonSignatures()[static_cast<std::size_t> (choice - 1)];
                            if (existingIndex >= 0)
                            {
                                if (const auto result = signature.setTimeSignature (existingIndex, chosen); ! result)
                                    reportError (result.error());
                            }
                            else if (const auto inserted = signature.insertTimeSignature (seconds, chosen); ! inserted)
                            {
                                reportError (inserted.error());
                            }
                        });
}

void SignatureTrack::showKeyChooser (juce::Rectangle<int> screenArea, double seconds, int existingIndex)
{
    // Neuer Punkt entsteht sofort mit der Tonart an dieser Stelle; die Sprechblase ändert ihn dann.
    int index = existingIndex;

    if (index < 0)
    {
        const auto current = signature.getKeyAt (seconds);
        const auto inserted = signature.insertKey (seconds, current.rootNote, current.scale);

        if (! inserted)
        {
            reportError (inserted.error());
            return;
        }

        index = *inserted;
    }

    const auto key = index < static_cast<int> (keyPoints.size()) ? keyPoints[static_cast<std::size_t> (index)]
                                                                 : signature.getKeyAt (seconds);

    auto chooser =
        std::make_unique<KeyChooser> (key.rootNote,
                                      key.scale,
                                      [this, index] (int rootNote, services::ScaleKind scale)
                                      {
                                          if (const auto result = signature.setKey (index, rootNote, scale); ! result)
                                              reportError (result.error());
                                      });
    juce::CallOutBox::launchAsynchronously (std::move (chooser), screenArea, nullptr);
}

void SignatureTrack::reportError (const core::Error& error) const
{
    juce::Logger::writeToLog ("Signaturspur: " + services::errorText (error));
}

} // namespace fk::ui
