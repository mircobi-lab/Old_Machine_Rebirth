#include "OscSurface.h"

OscSurface::OscSurface()
{
    receiver.addListener (this);
}

OscSurface::~OscSurface()
{
    stop();
    receiver.removeListener (this);
}

bool OscSurface::start (int receivePort, juce::String sendHost, int sendPort)
{
    stop();

    inPort = receivePort;
    outPort = sendPort;
    host = std::move (sendHost);

    listening = receiver.connect (receivePort);
    sending = sender.connect (host, sendPort);
    return listening;
}

void OscSurface::stop()
{
    if (listening)
        receiver.disconnect();
    if (sending)
        sender.disconnect();
    listening = sending = false;
}

juce::String OscSurface::description() const
{
    if (! listening)
        return "OSC SPENTO";
    return "OSC " + juce::String (inPort) + " > " + juce::String (outPort);
}

int OscSurface::intArg (const juce::OSCMessage& m, int index, int fallback)
{
    if (index >= m.size())
        return fallback;
    const auto& a = m[index];
    if (a.isInt32())   return a.getInt32();
    if (a.isFloat32()) return juce::roundToInt (a.getFloat32());
    if (a.isString())  return a.getString().getIntValue();
    return fallback;
}

void OscSurface::oscMessageReceived (const juce::OSCMessage& m)
{
    const auto address = m.getAddressPattern().toString();

    // TouchOSC e gli altri programmi semplici non sanno mandare argomenti in
    // testa: mettono tutto nell'indirizzo, /pad/3/13 con un valore solo. Qui si
    // accettano tutte e due le forme.
    if (address.startsWith ("/pad/") || address.startsWith ("/button/")
        || address.startsWith ("/encoder/") || address.startsWith ("/tempo/")
        || address.startsWith ("/swing/") || address.startsWith ("/volume/"))
    {
        auto parts = juce::StringArray::fromTokens (address.substring (1), "/", "");
        const bool pressed = m.size() == 0 || intArg (m, 0, 1) != 0
                             || (m[0].isFloat32() && m[0].getFloat32() > 0.0f);

        if (parts[0] == "pad" && parts.size() >= 3)
        {
            machine::PadRef p;
            p.bank = juce::jlimit (1, machine::numBanks, parts[1].getIntValue()) - 1;
            p.pad  = juce::jlimit (1, machine::padsPerBank, parts[2].getIntValue());

            if (pressed)
            {
                if (onPadDown != nullptr)
                    onPadDown (p, 100);
            }
            else if (onPadUp != nullptr)
                onPadUp (p);
            return;
        }

        if (parts[0] == "button" && parts.size() >= 2)
        {
            const auto c = control::forButton (parts[1].getIntValue());
            if (c.command != control::Command::none && onCommand != nullptr)
                onCommand (c, pressed);
            return;
        }

        if (! pressed)
            return;   // i passi si danno quando si preme, non quando si lascia

        if (parts[0] == "encoder" && parts.size() >= 3)
        {
            const int index = juce::jlimit (1, 8, parts[1].getIntValue()) - 1;
            if (onEncoder != nullptr)
                onEncoder (control::forEncoder (71 + index), parts[2].getIntValue());
            return;
        }

        if (parts.size() >= 2 && onEncoder != nullptr)
        {
            const int cc = parts[0] == "tempo" ? 14 : (parts[0] == "swing" ? 15 : 79);
            onEncoder (control::forEncoder (cc), parts[1].getIntValue());
        }
        return;
    }

    if (address == "/pad")
    {
        machine::PadRef p;
        p.bank = juce::jlimit (1, machine::numBanks, intArg (m, 0, 1)) - 1;
        p.pad  = juce::jlimit (1, machine::padsPerBank, intArg (m, 1, 1));
        const int velocity = juce::jlimit (0, 127, intArg (m, 2, 100));

        if (velocity > 0)
        {
            if (onPadDown != nullptr)
                onPadDown (p, velocity);
        }
        else if (onPadUp != nullptr)
            onPadUp (p);
        return;
    }

    if (address == "/button")
    {
        const auto c = control::forButton (intArg (m, 0, -1));
        if (c.command != control::Command::none && onCommand != nullptr)
            onCommand (c, intArg (m, 1, 1) > 0);
        return;
    }

    if (address == "/encoder")
    {
        const int index = juce::jlimit (1, 8, intArg (m, 0, 1)) - 1;
        const auto c = control::forEncoder (71 + index);
        if (onEncoder != nullptr)
            onEncoder (c, intArg (m, 1, 0));
        return;
    }

    auto wheel = [this, &m] (int cc)
    {
        const auto c = control::forEncoder (cc);
        if (onEncoder != nullptr)
            onEncoder (c, intArg (m, 0, 0));
    };

    if (address == "/tempo")  { wheel (14); return; }
    if (address == "/swing")  { wheel (15); return; }
    if (address == "/volume") { wheel (79); return; }
}

void OscSurface::sendPadLight (machine::PadRef p, const juce::String& state)
{
    if (! sending)
        return;
    sender.send ("/led/pad", p.bank + 1, p.pad, state);

    // forma per TouchOSC: un indirizzo per pad, acceso o spento
    const float level = state == "off" ? 0.0f : (state == "empty" ? 0.15f
                                                                  : (state == "playing" ? 1.0f : 0.6f));
    sender.send (juce::OSCAddressPattern ("/led/pad/" + juce::String (p.bank + 1)
                                           + "/" + juce::String (p.pad)), level);
}

void OscSurface::sendButtonLight (int cc, int value)
{
    if (! sending)
        return;
    sender.send ("/led/button", cc, value);
    sender.send (juce::OSCAddressPattern ("/led/button/" + juce::String (cc)),
                 juce::jlimit (0.0f, 1.0f, (float) value / 127.0f));
}

void OscSurface::sendScreen (const juce::StringArray& lines)
{
    if (! sending)
        return;

    int field = 1;
    for (const auto& line : lines)
    {
        if (line.startsWithChar ('#'))
        {
            sender.send ("/screen/title", line.substring (1).trim());
            continue;
        }
        const auto label = line.upToFirstOccurrenceOf ("|", false, false).trim();
        const auto value = line.fromFirstOccurrenceOf ("|", false, false).trim();
        sender.send ("/screen/field", field, label, value);
        sender.send (juce::OSCAddressPattern ("/screen/field/" + juce::String (field)),
                     label + "  " + value);
        ++field;
    }
}
