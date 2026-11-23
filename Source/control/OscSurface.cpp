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
}

void OscSurface::sendButtonLight (int cc, int value)
{
    if (! sending)
        return;
    sender.send ("/led/button", cc, value);
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
        sender.send ("/screen/field", field++,
                     line.upToFirstOccurrenceOf ("|", false, false).trim(),
                     line.fromFirstOccurrenceOf ("|", false, false).trim());
    }
}
