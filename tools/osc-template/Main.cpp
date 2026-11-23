// Genera il banco di comandi per Open Stage Control, cosi' la macchina si suona
// anche senza il Push 2: dal telefono, dal tablet o da un'altra finestra.
//
// Il file nasce dalla stessa tabella dei comandi che usa il Push, quindi non
// puo' andare fuori passo. Si rigenera con:
//
//     cmake --build build --target OscTemplate
//     ./build/OscTemplate_artefacts/OscTemplate tools/osc/old-machine-rebirth.json

#include "TouchOsc.h"

#include "control/ControlMap.h"
#include "machine/PadGrid.h"

#include <juce_core/juce_core.h>
#include <iostream>

namespace
{
using Obj = juce::DynamicObject;

juce::var widget (const juce::String& type, const juce::String& id)
{
    auto* o = new Obj();
    o->setProperty ("type", type);
    o->setProperty ("id", id);
    o->setProperty ("linkId", "");
    o->setProperty ("css", "");
    return juce::var (o);
}

void set (juce::var& v, const juce::String& name, const juce::var& value)
{
    v.getDynamicObject()->setProperty (name, value);
}

juce::var panel (const juce::String& id, const juce::String& label, const juce::String& layout,
                 const juce::Array<juce::var>& children)
{
    auto p = widget ("panel", id);
    set (p, "label", label);
    set (p, "layout", layout);
    set (p, "width", "100%");
    set (p, "height", "100%");
    set (p, "scroll", false);
    set (p, "innerPadding", true);
    set (p, "widgets", children);
    return p;
}

// Un pad: manda /pad banco pad velocity, e si accende da /led/pad
juce::var padWidget (int bank, int pad)
{
    auto w = widget ("push", "pad_" + juce::String (bank) + "_" + juce::String (pad));
    set (w, "label", machine::bankName (bank - 1) + juce::String (pad));
    set (w, "address", "/pad");
    juce::Array<juce::var> pre { bank, pad };
    set (w, "preArgs", pre);
    set (w, "on", 100);
    set (w, "off", 0);
    set (w, "mode", "push");
    set (w, "colorText", "auto");
    return w;
}

juce::var buttonWidget (const control::Control& c)
{
    auto w = widget ("push", "cc_" + juce::String (c.number));
    set (w, "label", c.label);
    set (w, "address", "/button");
    juce::Array<juce::var> pre { c.number };
    set (w, "preArgs", pre);
    set (w, "on", 1);
    set (w, "off", 0);
    set (w, "mode", "push");
    return w;
}

// I rotativi del Push mandano passi, non valori assoluti: qui due tastini
juce::var stepPair (const juce::String& address, const juce::String& label, int index)
{
    juce::Array<juce::var> children;
    for (const int step : { -1, 1 })
    {
        auto w = widget ("push", "step_" + label.toLowerCase().replace (" ", "_")
                                   + (step < 0 ? "_giu" : "_su"));
        set (w, "label", label + (step < 0 ? " -" : " +"));
        set (w, "address", address);
        juce::Array<juce::var> pre;
        if (index > 0)
            pre.add (index);
        pre.add (step);
        set (w, "preArgs", pre);
        set (w, "on", step);
        set (w, "mode", "push");
        children.add (w);
    }
    return panel ("rot_" + label.toLowerCase().replace (" ", "_"), label, "horizontal", children);
}

juce::var screenField (int index)
{
    auto w = widget ("text", "campo_" + juce::String (index));
    set (w, "address", "/screen/field");
    juce::Array<juce::var> pre { index };
    set (w, "preArgs", pre);
    set (w, "value", "OSC{/screen/field " + juce::String (index) + ", -}");
    set (w, "vertical", false);
    return w;
}

juce::var tab (const juce::String& id, const juce::String& label, const juce::Array<juce::var>& children,
               const juce::String& layout)
{
    auto t = widget ("tab", id);
    set (t, "label", label);
    set (t, "layout", layout);
    set (t, "widgets", children);
    return t;
}
}

int main (int argc, char** argv)
{
    const juce::String path = argc > 1 ? juce::String (argv[1])
                                       : juce::String ("tools/osc/old-machine-rebirth.json");

    // --- i pad, quattro banchi a quadranti come sul Push
    juce::Array<juce::var> rows;
    for (int row = 0; row < 8; ++row)
    {
        juce::Array<juce::var> cells;
        for (int col = 0; col < 8; ++col)
        {
            const auto p = machine::padForCell (row, col, 0);
            cells.add (padWidget (p.bank + 1, p.pad));
        }
        rows.add (panel ("riga_" + juce::String (row), "", "horizontal", cells));
    }
    auto pads = panel ("griglia", "PAD", "vertical", rows);

    // --- le pagine, i tasti funzione e la quantizzazione
    juce::Array<juce::var> pagine, funzioni, quantizza, trasporto, altro;
    for (const auto& c : control::buttons())
    {
        using C = control::Command;
        switch (c.command)
        {
            case C::pageSample: case C::pageChop: case C::pageSound: case C::pageSequence:
            case C::pageStep: case C::pageSong: case C::pageMix: case C::pageDisk:
                pagine.add (buttonWidget (c)); break;
            case C::softKey:
                funzioni.add (buttonWidget (c)); break;
            case C::quantiseValue:
                quantizza.add (buttonWidget (c)); break;
            case C::play: case C::stop: case C::recordSequence: case C::overdub:
            case C::recordSample: case C::newSequence: case C::duplicate: case C::fixedLength:
                trasporto.add (buttonWidget (c)); break;
            default:
                altro.add (buttonWidget (c)); break;
        }
    }

    juce::Array<juce::var> pannello {
        panel ("pagine", "PAGINE", "horizontal", pagine),
        panel ("funzioni", "TASTI FUNZIONE", "horizontal", funzioni),
        panel ("trasporto", "TRASPORTO", "horizontal", trasporto),
        panel ("quantizza", "QUANTIZZA", "horizontal", quantizza)
    };

    juce::Array<juce::var> rotativi {
        stepPair ("/tempo", "TEMPO", 0),
        stepPair ("/swing", "SWING", 0),
        stepPair ("/volume", "VOLUME", 0)
    };
    for (int i = 1; i <= 8; ++i)
        rotativi.add (stepPair ("/encoder", "P" + juce::String (i), i));

    juce::Array<juce::var> schermo;
    {
        auto titolo = widget ("text", "titolo");
        set (titolo, "address", "/screen/title");
        set (titolo, "value", "OSC{/screen/title, OLD MACHINE REBIRTH}");
        schermo.add (titolo);
        for (int i = 1; i <= 8; ++i)
            schermo.add (screenField (i));
    }

    juce::Array<juce::var> tabs {
        tab ("suona", "SUONA", { pads }, "vertical"),
        tab ("pannello", "PANNELLO", pannello, "vertical"),
        tab ("rotativi", "ROTATIVI", rotativi, "grid"),
        tab ("schermo", "SCHERMO", schermo, "vertical"),
        tab ("resto", "ALTRO", { panel ("resto_p", "ALTRI TASTI", "grid", altro) }, "vertical")
    };

    auto root = widget ("root", "root");
    set (root, "type", "session");
    set (root, "createdWith", "Old Machine Rebirth");
    set (root, "color", "auto");
    set (root, "address", "/root");
    set (root, "preArgs", "");
    set (root, "target", "");
    set (root, "bypass", false);
    set (root, "tabs", tabs);

    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    file.getParentDirectory().createDirectory();
    if (! file.replaceWithText (juce::JSON::toString (root, false)))
    {
        std::cerr << "non riesco a scrivere " << file.getFullPathName() << std::endl;
        return 1;
    }

    std::cout << "scritto " << file.getFullPathName() << std::endl;

    // e il banco per TouchOSC Mk1, accanto all'altro
    const auto touch = file.getSiblingFile (file.getFileNameWithoutExtension() + ".touchosc");
    if (! touchosc::write (touch))
    {
        std::cerr << "non riesco a scrivere " << touch.getFullPathName() << std::endl;
        return 1;
    }
    std::cout << "scritto " << touch.getFullPathName() << std::endl;
    return 0;
}
