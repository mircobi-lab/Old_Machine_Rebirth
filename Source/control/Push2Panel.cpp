#include "Push2Panel.h"

#include "push2/Push2Protocol.h"
#include "push2/MachineScreen.h"

namespace
{
struct Rgb { std::uint8_t r, g, b; };

// La tavolozza che scriviamo nel Push.
//
// Le prime due voci sono gli stati che non dipendono dal suono: il pad vuoto e
// quello mutato. Dopo vengono tre voci per ogni famiglia (fermo, scelto, che
// suona), cosi' la batteria resta grigia e le voci restano celesti anche
// mentre si suona. In tutto 2 + 8*3 = 26 voci.
constexpr int firstPaletteIndex = 1;
constexpr int statesPerKind = 3;
constexpr Rgb emptyColour { 0x1b, 0x1d, 0x1f };     // vuoto, quasi spento
constexpr Rgb mutedColour { 0xff, 0x3b, 0x30 };     // mutato, rosso

Rgb toRgb (juce::Colour c)
{
    return { c.getRed(), c.getGreen(), c.getBlue() };
}

Rgb colourForEntry (int entry)
{
    if (entry == 0)
        return emptyColour;
    if (entry == 1)
        return mutedColour;

    const int which = entry - 2;
    const auto kind = sound::kindFromIndex (which / statesPerKind);

    switch (which % statesPerKind)
    {
        case 0:  return toRgb (sound::colour (kind));
        case 1:  return toRgb (sound::colourSelected (kind));
        default: return toRgb (sound::colourPlaying (kind));
    }
}
}

int Push2Panel::numberOfPaletteEntries()
{
    return 2 + sound::numKinds * statesPerKind;
}

int Push2Panel::paletteIndexFor (machine::PadLight l, sound::Kind kind)
{
    const int base = firstPaletteIndex + 2 + sound::indexOf (kind) * statesPerKind;

    switch (l)
    {
        case machine::PadLight::off:      return 0;             // nero, voce di serie
        case machine::PadLight::empty:    return firstPaletteIndex;
        case machine::PadLight::muted:    return firstPaletteIndex + 1;
        case machine::PadLight::loaded:   return base;
        case machine::PadLight::selected: return base + 1;
        case machine::PadLight::playing:  return base + 2;
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
    lights.fill (machine::PadLight::empty);
    kinds.fill (sound::Kind::generico);
    onScreen.fill (machine::PadLight::off);
    onScreenEntry.fill (-1);

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
        for (const auto& b : control::buttons())
            output->sendMessageNow (juce::MidiMessage::controllerEvent (1, b.number, 0));
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
        lightAllButtons();
        fullRefresh = true;
    }

    if (input != nullptr)
        input->start();
}

void Push2Panel::sendPalette()
{
    const int count = numberOfPaletteEntries();

    std::vector<juce::MidiMessage> out;
    for (int i = 0; i < count; ++i)
        out.push_back (sysexFor ({ 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x04,
                                   (std::uint8_t) (firstPaletteIndex + i), 0xF7 }));   // leggi
    for (int i = 0; i < count; ++i)
    {
        const auto c = colourForEntry (i);
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

void Push2Panel::lightAllButtons()
{
    // Tutti i tasti che la macchina usa stanno accesi smorti, cosi' si vede al
    // buio cosa c'e'; chi li accende forte e' la pagina aperta.
    for (const auto& b : control::buttons())
        setButtonLight (b.number, b.rgb ? paletteIndexFor (machine::PadLight::loaded,
                                                           sound::Kind::giro) : 24);
}

void Push2Panel::setButtonLight (int cc, int value)
{
    const juce::ScopedLock sl (lock);
    const auto it = buttonLights.find (cc);
    if (it != buttonLights.end() && it->second == value)
        return;
    buttonLights[cc] = value;
    if (output != nullptr)
        output->sendMessageNow (juce::MidiMessage::controllerEvent (1, cc, juce::jlimit (0, 127, value)));
}

void Push2Panel::setPadLight (machine::PadRef p, machine::PadLight l, sound::Kind kind)
{
    const int i = machine::drumIndex (p);
    if (! juce::isPositiveAndBelow (i, machine::numDrums))
        return;
    const juce::ScopedLock sl (lock);
    lights[(size_t) i] = l;
    kinds[(size_t) i] = kind;
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
            machine::PadLight want;
            sound::Kind kind;
            {
                const juce::ScopedLock sl (lock);
                want = lights[(size_t) machine::drumIndex (p)];
                kind = kinds[(size_t) machine::drumIndex (p)];
            }
            const int cell = row * 8 + col;
            const int entry = paletteIndexFor (want, kind);
            if (! everything && onScreenEntry[(size_t) cell] == entry)
                continue;
            onScreen[(size_t) cell] = want;
            onScreenEntry[(size_t) cell] = entry;
            out.push_back (juce::MidiMessage::noteOn (1, push2::padNote (row, col),
                                                      (juce::uint8) entry));
        }
    send (out);
}

void Push2Panel::setWave (mbstudio::WaveView view)
{
    const juce::ScopedLock sl (lock);
    wave = std::move (view);
}

void Push2Panel::drawScreen()
{
    if (display == nullptr)
        return;

    juce::StringArray lines;
    mbstudio::WaveView view;
    {
        const juce::ScopedLock sl (lock);
        lines = screenLines;
        view = wave;
    }
    auto image = mbstudio::renderMachineScreen (lines, machine::bankName (bankGroup * 4),
                                                machine::bankName (bankGroup * 4 + 3), font, view);
    display->setImage (image);
    {
        const juce::ScopedLock sl (lock);
        screenImage = image;
    }
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

    if (m.isController())
    {
        const auto c = control::forButton (m.getControllerNumber());
        if (c.command != control::Command::none)
        {
            if (onCommand != nullptr)
                onCommand (c, m.getControllerValue() > 0);
            return;
        }

        const auto e = control::forEncoder (m.getControllerNumber());
        if (e.command != control::Command::none && onEncoder != nullptr)
            onEncoder (e, push2::encoderDelta (m.getControllerValue()));
        return;
    }

    if (m.isPitchWheel())
    {
        // la striscia sotto il display manda il pitch bend: 0 in basso, tutto
        // in cima
        if (onTouchStrip != nullptr)
            onTouchStrip ((float) m.getPitchWheelValue() / 16383.0f);
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

juce::Image Push2Panel::lastScreen() const
{
    const juce::ScopedLock sl (lock);
    return screenImage;
}
