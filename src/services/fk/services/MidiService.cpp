#include "fk/services/MidiService.h"

namespace fk::services
{

MidiService::MidiService (engine::EngineContext& context)
    : engineContext (context)
    , sourceId (te::createUniqueMPESourceID())
{
    engineContext.getDeviceManager().addChangeListener (this);
}

MidiService::~MidiService()
{
    engineContext.getDeviceManager().removeChangeListener (this);
}

juce::String MidiService::getVirtualKeyboardName()
{
    return "Bildschirm-Keyboard";
}

juce::String MidiService::getAllInputsId()
{
    return "all_midi_in"; // Tracktions feste Kennung des Sammelgeräts
}

juce::String MidiService::getAllInputsName()
{
    return TRANS ("All MIDI inputs");
}

std::vector<MidiDeviceInfo> MidiService::getInputDevices() const
{
    std::vector<MidiDeviceInfo> devices;

    for (const auto& device : engineContext.getDeviceManager().getMidiInDevices())
    {
        const bool isAllInputs = device->getDeviceID() == getAllInputsId();
        devices.push_back ({ device->getDeviceID(),
                             isAllInputs ? getAllInputsName() : device->getName(),
                             device->isEnabled(),
                             device->getDeviceType() == te::InputDevice::virtualMidiDevice });
    }

    return devices;
}

void MidiService::setInputDeviceEnabled (const juce::String& deviceId, bool shouldBeEnabled)
{
    for (const auto& device : engineContext.getDeviceManager().getMidiInDevices())
        if (device->getDeviceID() == deviceId && device->isEnabled() != shouldBeEnabled)
        {
            device->setEnabled (shouldBeEnabled);
            listeners.call (&Listener::midiDevicesChanged);
        }
}

void MidiService::rescanDevices()
{
    engineContext.getDeviceManager().rescanMidiDeviceList();
}

void MidiService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void MidiService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void MidiService::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Tracktion meldet sich nach jedem Abgleich der Geräteliste (Hot-Plug, Rescan, Umschalten).
    listeners.call (&Listener::midiDevicesChanged);
}

void MidiService::ensureVirtualKeyboardExists()
{
    auto& deviceManager = engineContext.getDeviceManager();

    // Tracktion merkt sich virtuelle Geräte in den Einstellungen. Anlegen und ein
    // "gibt es schon" stillschweigend hinnehmen: Beides ist das gewünschte Ergebnis.
    [[maybe_unused]] const auto result = deviceManager.createVirtualMidiDevice (getVirtualKeyboardName());

    // Nur ein erfolgreiches Anlegen stößt Tracktions (asynchronen) Rescan an. Gab es das Gerät
    // schon, muss die Liste trotzdem aufgebaut werden, sonst bleibt sie bis zum ersten Timer leer.
    deviceManager.rescanMidiDeviceList();
}

void MidiService::sendNoteOn (int noteNumber, float velocity)
{
    send (juce::MidiMessage::noteOn (1, noteNumber, velocity));
}

void MidiService::sendNoteOff (int noteNumber)
{
    send (juce::MidiMessage::noteOff (1, noteNumber));
}

void MidiService::send (const juce::MidiMessage& message)
{
    for (const auto& device : engineContext.getDeviceManager().getMidiInDevices())
    {
        if (device->getName() == getVirtualKeyboardName())
        {
            if (! device->isEnabled())
                device->setEnabled (true);

            // Tracktion ordnet die Nachricht über ihren Zeitstempel in die Aufnahme ein; er muss in
            // derselben Uhr laufen wie der Audio-Stream (Sekunden seit Programmstart). Ohne Stempel (0)
            // landet jede Note weit vor dem Projektanfang und wird verworfen.
            auto stamped = message;
            stamped.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
            device->handleIncomingMidiMessage (stamped, sourceId);
            return;
        }
    }
}

} // namespace fk::services
