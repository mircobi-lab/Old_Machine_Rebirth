#include "Push2Panel.h"

#include "push2/Push2Protocol.h"
#include "push2/MachineScreen.h"

namespace
{
struct Rgb { std::uint8_t r, g, b; };

// I colori dei LED dello stile MBStudio: il colore sta solo nei LED
constexpr int firstPaletteIndex = 1;
constexpr Rgb paletteColours[] = {
    { 0x1b, 0x1d, 0x1f },   // vuoto, quasi spento
    { 0x3d, 0x8b, 0xff },   // caricato, blu
    { 0xff, 0xb0, 0x20 },   // scelto, ambra
    { 0xf3, 0xf3, 0xf1 },   // che suona, bianco
};
constexpr int numPaletteEntries = (int) (sizeof (paletteColours) / sizeof (Rgb));
}

int Push2Panel::paletteIndexFor (PadLight l)
{
    switch (l)
    {
        case PadLight::off:      return 0;                      // nero, voce di serie
        case PadLight::empty:    return firstPaletteIndex;
        case PadLight::loaded:   return firstPaletteIndex + 1;
        case PadLight::selected: return firstPaletteIndex + 2;
        case PadLight::playing:  return firstPaletteIndex + 3;
    }
    return 0;
}

std::optional<juce::MidiDeviceInfo> Push2Panel::choosePort (const juce::Array<juce::MidiDeviceInfo>& list,
                                                            bool preferUserPort)
{
    const char* wanted = preferUserPort ? "User" : "Live";
    std::optional<juce::MidiDeviceInfo> fallback;

    for (const auto& d : list)
    {
        if (! d.name.containsIgnoreCase ("Push 2"))
            continue;
        if (d.name.containsIgnoreCase (wanted))
            return d;
        if (! fallback.has_value())
            fallback = d;
    }
    return fallback;
}

juce::MidiMessage Push2Panel::sysexFor (const std::vector<std::uint8_t>& b)
{
    return juce::MidiMessage::createSysExMessage (b.data() + 1, (int) b.size() - 2);
}

Push2Panel::Push2Panel (juce::Typeface::Ptr displayFont)
    : font (std::move (displayFont))
{
    lights.fill (PadLight::empty);
    onScreen.fill (PadLight::off);

    display = std::make_unique<Push2Display>();
    reconnect();
    devicesChanged = juce::MidiDeviceListConnection::make ([this] { reconnect(); });
    startTimerHz (20);
}

Push2Panel::~Push2Panel()
{
    stopTimer();

    if (output != nullptr)
    {
        for (int i = 36; i <= 99; ++i)
            output->sendMessageNow (juce::MidiMessage::noteOn (1, i, (juce::uint8) 0));
        restorePalette();
        output->sendMessageNow (sysexFor (push2::setMidiMode (push2::midiModeLive)));
    }

    if (input != nullptr)
        input->stop();
}

void Push2Panel::reconnect()
{
    const auto in = choosePort (juce::MidiInput::getAvailableDevices(), true);
    const auto out = choosePort (juce::MidiOutput::getAvailableDevices(), true);
    const auto inId = in.has_value() ? in->identifier : juce::String();
    const auto outId = out.has_value() ? out->identifier : juce::String();

    const bool same = input != nullptr && output != nullptr
                      && input->getIdentifier() == inId && output->getIdentifier() == outId;
    if (same)
        return;

    if (output != nullptr)
        restorePalette();
    if (input != nullptr)
        input->stop();

    input = nullptr;
    output = nullptr;
    portName.clear();

    if (inId.isEmpty() || outId.isEmpty())
        return;

    input = juce::MidiInput::openDevice (inId, this);
    output = juce::MidiOutput::openDevice (outId);

    if (output != nullptr)
    {
        portName = out->name.contains ("User") ? "PORTA USER" : "PORTA LIVE";
        // Appena acceso il Push e' in Live mode e ignora quello che arriva sulla
        // porta User; il sysex pero' lo ascolta sempre, da tutte e due le porte.
        output->sendMessageNow (sysexFor (push2::setMidiMode (push2::midiModeUser)));
        sendPalette();
        fullRefresh = true;
    }

    if (input != nullptr)
        input->start();
}

void Push2Panel::sendPalette()
{
    std::vector<juce::MidiMessage> out;
    for (int i = 0; i < numPaletteEntries; ++i)
        out.push_back (sysexFor ({ 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x04,
                                   (std::uint8_t) (firstPaletteIndex + i), 0xF7 }));   // leggi
    for (int i = 0; i < numPaletteEntries; ++i)
    {
        const auto& c = paletteColours[i];
        const auto white = (std::uint8_t) ((c.r * 2 + c.g * 5 + c.b) / 8);
        out.push_back (sysexFor (push2::setPaletteEntry (firstPaletteIndex + i, c.r, c.g, c.b, white)));
    }
    out.push_back (sysexFor (push2::reapplyPalette()));
    send (out);
}

void Push2Panel::restorePalette()
{
    if (originalPalette.empty() || output == nullptr)
        return;
    send (originalPalette);
    send ({ sysexFor (push2::reapplyPalette()) });
    originalPalette.clear();
}

void Push2Panel::send (const std::vector<juce::MidiMessage>& messages)
{
    if (output == nullptr)
        return;
    for (const auto& m : messages)
        output->sendMessageNow (m);
}

void Push2Panel::setPadLight (machine::PadRef p, PadLight l)
{
    const int i = machine::drumIndex (p);
    if (! juce::isPositiveAndBelow (i, machine::numDrums))
        return;
    const juce::ScopedLock sl (lock);
    lights[(size_t) i] = l;
}

void Push2Panel::setBankGroup (int group)
{
    const int g = juce::jlimit (0, 1, group);
    if (g == bankGroup)
        return;
    bankGroup = g;
    fullRefresh = true;
    if (onBankGroupChanged != nullptr)
        onBankGroupChanged (g);
}

void Push2Panel::setScreenLines (juce::StringArray lines)
{
    const juce::ScopedLock sl (lock);
    screenLines = std::move (lines);
}

void Push2Panel::timerCallback()
{
    refreshLeds (fullRefresh);
    fullRefresh = false;
    drawScreen();
}

void Push2Panel::refreshLeds (bool everything)
{
    if (output == nullptr)
        return;

    std::vector<juce::MidiMessage> out;
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
        {
            const auto p = machine::padForCell (row, col, bankGroup);
            PadLight want;
            {
                const juce::ScopedLock sl (lock);
                want = lights[(size_t) machine::drumIndex (p)];
            }
            const int cell = row * 8 + col;
            if (! everything && onScreen[(size_t) cell] == want)
                continue;
            onScreen[(size_t) cell] = want;
            out.push_back (juce::MidiMessage::noteOn (1, push2::padNote (row, col),
                                                      (juce::uint8) paletteIndexFor (want)));
        }
    send (out);
}

void Push2Panel::drawScreen()
{
    if (display == nullptr)
        return;

    juce::StringArray lines;
    {
        const juce::ScopedLock sl (lock);
        lines = screenLines;
    }
    display->setImage (mbstudio::renderMachineScreen (lines, machine::bankName (bankGroup * 4),
                                                      machine::bankName (bankGroup * 4 + 3), font));
}

void Push2Panel::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.isSysEx())
    {
        const auto* d = m.getSysExData();
        const int n = m.getSysExDataSize();
        if (n == 13 && d[0] == 0x00 && d[1] == 0x21 && d[2] == 0x1D && d[5] == 0x04)
        {
            std::vector<juce::uint8> entry (d, d + n);
            entry[5] = 0x03;    // la stessa voce, ma come comando "scrivi"
            originalPalette.push_back (juce::MidiMessage::createSysExMessage (entry.data(), n));
        }
        return;
    }

    if (m.isNoteOnOrOff())
    {
        const auto pos = push2::padPosition (m.getNoteNumber());
        if (! pos.has_value())
            return;

        const auto p = machine::padForCell (pos->first, pos->second, bankGroup);
        const int velocity = m.getVelocity();

        if (m.isNoteOn() && velocity > 0)
        {
            if (onPadDown != nullptr)
                onPadDown (p, velocity);
        }
        else if (onPadUp != nullptr)
            onPadUp (p);
    }
}
