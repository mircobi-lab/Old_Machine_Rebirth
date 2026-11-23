#include "TouchOsc.h"

#include "control/ControlMap.h"
#include "machine/PadGrid.h"

namespace touchosc
{
namespace
{
// Schermo dell'iPhone in verticale, come lo tratta TouchOSC Mk1
constexpr int pageWidth = 320, pageHeight = 480;

juce::String b64 (const juce::String& text)
{
    return juce::Base64::toBase64 (text);
}

juce::String attr (const juce::String& name, const juce::String& value)
{
    return " " + name + "=\"" + value + "\"";
}

// Un comando qualsiasi: tasto, pad, etichetta
juce::String control (const juce::String& type, const juce::String& name, const juce::String& address,
                      int x, int y, int w, int h, const juce::String& colour,
                      const juce::String& extra = {})
{
    juce::String s;
    s << "<control"
      << attr ("name", b64 (name))
      << attr ("x", juce::String (x))
      << attr ("y", juce::String (y))
      << attr ("w", juce::String (w))
      << attr ("h", juce::String (h))
      << attr ("color", colour)
      << attr ("scalef", "0.0")
      << attr ("scalet", "1.0")
      << attr ("type", type);
    // TouchOSC vuole un indirizzo su ogni comando, anche su quelli che non
    // mandano niente: alle etichette ferme se ne da' uno finto
    s << attr ("osc_cs", b64 (address.isNotEmpty() ? address : "/scritta/" + name));
    s << extra << "></control>";
    return s;
}

juce::String label (const juce::String& name, const juce::String& address, const juce::String& text,
                    int x, int y, int w, int h, int size, const juce::String& colour = "gray")
{
    return control ("labelh", name, address, x, y, w, h, colour,
                    attr ("text", b64 (text)) + attr ("size", juce::String (size))
                        + attr ("background", "false") + attr ("outline", "false"));
}

juce::String button (const juce::String& name, const juce::String& address, const juce::String& text,
                     int x, int y, int w, int h, const juce::String& colour)
{
    juce::String s;
    s << control ("push", name, address, x, y, w, h, colour,
                  attr ("local_off", "false") + attr ("response", "absolute"));
    // la scritta sopra il tasto: TouchOSC Mk1 non la mette dentro il push
    s << label (name + "_t", {}, text, x, y + h / 2 - 8, w, 16, 12, colour);
    return s;
}

juce::String page (const juce::String& name, const juce::String& body)
{
    return "<tabpage" + attr ("name", b64 (name)) + attr ("scalef", "0.0") + attr ("scalet", "1.0")
           + ">" + body + "</tabpage>";
}

juce::String padsPage()
{
    juce::String s;
    s << label ("titolo", "/screen/title", "OLD MACHINE REBIRTH", 4, 2, 312, 20, 16, "gray");
    s << label ("campo1", "/screen/field/1", "PAD", 4, 24, 150, 16, 12, "gray");
    s << label ("campo3", "/screen/field/3", "QUANTIZZA", 160, 24, 156, 16, 12, "gray");

    const int top = 46, side = 38, gap = 2;
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
        {
            const auto p = machine::padForCell (row, col, 0);
            const auto id = "pad_" + juce::String (p.bank + 1) + "_" + juce::String (p.pad);
            const auto address = "/pad/" + juce::String (p.bank + 1) + "/" + juce::String (p.pad);
            // un colore per quadrante, come i banchi
            const char* colour = p.bank == 0 ? "blue" : (p.bank == 1 ? "green"
                                                      : (p.bank == 2 ? "orange" : "purple"));
            s << control ("push", id, address,
                          4 + col * (side + gap), top + row * (side + gap), side, side, colour,
                          attr ("local_off", "false") + attr ("response", "absolute"));
            s << control ("led", "led_" + id, "/led/pad/" + juce::String (p.bank + 1) + "/"
                                                 + juce::String (p.pad),
                          4 + col * (side + gap), top + row * (side + gap), 10, 10, colour);
        }

    s << button ("banchi", "/button/31", "BANCHI A-D / E-H", 4, 414, 156, 30, "yellow");
    s << button ("stop", "/button/29", "STOP", 164, 414, 70, 30, "red");
    s << button ("play", "/button/85", "PLAY", 240, 414, 76, 30, "green");
    return s;
}

juce::String panelPage()
{
    juce::String s;
    int y = 4;

    auto row = [&s, &y] (const juce::Array<control::Control>& items, const juce::String& colour,
                         int perRow, int height)
    {
        const int w = (320 - 8 - (perRow - 1) * 2) / perRow;
        int i = 0;
        for (const auto& c : items)
        {
            const int col = i % perRow;
            if (col == 0 && i > 0)
                y += height + 2;
            s << button ("b" + juce::String (c.number), "/button/" + juce::String (c.number),
                         c.label, 4 + col * (w + 2), y, w, height, colour);
            ++i;
        }
        y += height + 8;
    };

    juce::Array<control::Control> pagine, funzioni, trasporto, quantizza;
    for (const auto& c : control::buttons())
    {
        using C = control::Command;
        switch (c.command)
        {
            case C::pageSample: case C::pageChop: case C::pageSound: case C::pageSequence:
            case C::pageStep: case C::pageSong: case C::pageMix: case C::pageDisk:
                pagine.add (c); break;
            case C::softKey: funzioni.add (c); break;
            case C::quantiseValue: quantizza.add (c); break;
            case C::play: case C::stop: case C::recordSequence: case C::overdub:
            case C::newSequence: case C::duplicate: case C::fixedLength: case C::timingCorrect:
                trasporto.add (c); break;
            default: break;
        }
    }

    s << label ("l_pagine", {}, "PAGINE", 4, y, 312, 14, 11, "gray"); y += 16;
    row (pagine, "blue", 4, 40);
    s << label ("l_funzioni", {}, "TASTI FUNZIONE", 4, y, 312, 14, 11, "gray"); y += 16;
    row (funzioni, "gray", 4, 34);
    s << label ("l_trasporto", {}, "TRASPORTO", 4, y, 312, 14, 11, "gray"); y += 16;
    row (trasporto, "green", 4, 34);
    s << label ("l_quantizza", {}, "QUANTIZZA", 4, y, 312, 14, 11, "gray"); y += 16;
    row (quantizza, "orange", 4, 30);
    return s;
}

juce::String knobsPage()
{
    juce::String s;
    int y = 4;

    auto pair = [&s, &y] (const juce::String& address, const juce::String& name)
    {
        s << label ("l_" + name, {}, name, 4, y + 10, 100, 16, 13, "gray");
        s << button ("giu_" + name, address + "/-1", "-", 108, y, 100, 36, "gray");
        s << button ("su_" + name, address + "/1", "+", 212, y, 104, 36, "gray");
        y += 40;
    };

    pair ("/tempo", "TEMPO");
    pair ("/swing", "SWING");
    pair ("/volume", "VOLUME");
    for (int i = 1; i <= 8; ++i)
        pair ("/encoder/" + juce::String (i), "P" + juce::String (i));
    return s;
}

juce::String screenPage()
{
    juce::String s;
    s << label ("s_titolo", "/screen/title", "OLD MACHINE REBIRTH", 4, 8, 312, 28, 20, "gray");
    for (int i = 1; i <= 8; ++i)
        s << label ("s_campo" + juce::String (i), "/screen/field/" + juce::String (i), "-",
                    4, 44 + (i - 1) * 30, 312, 26, 15, "gray");
    return s;
}

juce::String otherPage()
{
    juce::String s;
    int i = 0;
    for (const auto& c : control::buttons())
    {
        using C = control::Command;
        switch (c.command)
        {
            case C::pageSample: case C::pageChop: case C::pageSound: case C::pageSequence:
            case C::pageStep: case C::pageSong: case C::pageMix: case C::pageDisk:
            case C::softKey: case C::quantiseValue:
            case C::play: case C::stop: case C::recordSequence: case C::overdub:
            case C::newSequence: case C::duplicate: case C::fixedLength: case C::timingCorrect:
                continue;
            default: break;
        }

        const int col = i % 3, rowIndex = i / 3;
        s << button ("o" + juce::String (c.number), "/button/" + juce::String (c.number), c.label,
                     4 + col * 105, 4 + rowIndex * 38, 102, 34, "gray");
        ++i;
    }
    return s;
}
}

bool write (const juce::File& destination)
{
    juce::String xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        << "<layout" << attr ("version", "17") << attr ("mode", "0")
        << attr ("orientation", "vertical") << ">"
        << page ("PAD", padsPage())
        << page ("PANNELLO", panelPage())
        << page ("ROTATIVI", knobsPage())
        << page ("SCHERMO", screenPage())
        << page ("ALTRO", otherPage())
        << "</layout>";

    juce::ZipFile::Builder builder;
    builder.addEntry (new juce::MemoryInputStream (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), true),
                      9, "index.xml", juce::Time::getCurrentTime());

    destination.getParentDirectory().createDirectory();
    destination.deleteFile();
    juce::FileOutputStream out (destination);
    if (! out.openedOk())
        return false;

    return builder.writeToStream (out, nullptr);
}

} // namespace touchosc
