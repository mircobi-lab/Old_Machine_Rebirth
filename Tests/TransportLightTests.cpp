#include "control/Push2Panel.h"
#include "machine/Machine.h"

#include <juce_core/juce_core.h>

// Le luci dei tasti del trasporto, quelli in basso a destra del Push: verde il
// play, rosso la registrazione, bianco lo stop, e il lampeggio quando la
// macchina aspetta.
class TransportLightTests final : public juce::UnitTest
{
public:
    TransportLightTests() : juce::UnitTest ("Luci del trasporto", "Macchina") {}

    static control::Control find (control::Command wanted)
    {
        for (const auto& c : control::buttons())
            if (c.command == wanted)
                return c;
        return {};
    }

    static void press (machine::Machine& m, const control::Control& c)
    {
        m.command (c, true);
        m.command (c, false);
    }

    void runTest() override
    {
        using Lamp = machine::Machine::Lamp;

        beginTest ("i numeri dei tasti sono quelli del Push");
        {
            // dalla mappa ufficiale di Ableton: play 85, record 86, stop 29,
            // automate (il nostro sovraincidi) 89
            expectEquals (find (control::Command::play).number, 85);
            expectEquals (find (control::Command::recordSequence).number, 86);
            expectEquals (find (control::Command::stop).number, 29);
            expectEquals (find (control::Command::overdub).number, 89);
        }

        beginTest ("da fermo si accende lo stop, non il play");
        {
            machine::Machine m;
            m.prepare (44100.0);

            expect (m.lampFor (find (control::Command::stop)).state == Lamp::acceso);
            expect (m.lampFor (find (control::Command::play)).state == Lamp::smorto);
        }

        beginTest ("mentre gira il play e' verde e lo stop si smorza");
        {
            machine::Machine m;
            m.prepare (44100.0);
            press (m, find (control::Command::play));

            const auto play = m.lampFor (find (control::Command::play));
            expect (play.state == Lamp::acceso);
            expect (play.colour.getGreen() > play.colour.getRed() + 60, "il play non e' verde");

            expect (m.lampFor (find (control::Command::stop)).state == Lamp::smorto);
        }

        beginTest ("la registrazione e' rossa, e lampeggia quando aspetta");
        {
            machine::Machine m;
            m.prepare (44100.0);

            const auto rec = find (control::Command::recordSequence);
            expect (m.lampFor (rec).state == Lamp::smorto);

            press (m, rec);
            const auto lamp = m.lampFor (rec);
            expect (lamp.state == Lamp::acceso, "non si accende mentre registra");
            expect (lamp.colour.getRed() > lamp.colour.getGreen() + 60, "non e' rossa");

            // armata ma ferma: lampeggia
            m.sequencer().stop();
            m.sequencer().toggleRecord();
            if (m.sequencer().isRecording() && ! m.sequencer().isPlaying())
                expect (m.lampFor (rec).state == Lamp::lampeggia, "armata e ferma non lampeggia");
        }

        beginTest ("SHIFT e REC armano il campionatore, e il tasto lampeggia");
        {
            machine::Machine m;
            m.prepare (44100.0);

            const auto rec = find (control::Command::recordSequence);
            const auto shift = find (control::Command::shift);

            m.command (shift, true);                // tenuto giu'
            press (m, rec);
            m.command (shift, false);

            expect (m.recorder().isWaiting(), "il campionatore non si e' armato");
            expect (! m.sequencer().isRecording(), "ha registrato la sequenza invece");
            expect (m.lampFor (rec).state == Lamp::lampeggia,
                    "il tasto non lampeggia mentre aspetta il suono");
        }

        beginTest ("ogni colore ha il suo posto nella tavolozza");
        {
            const juce::Colour bianco (0xfff3f3f1), verde (0xff3ee05c), rosso (0xffff3b30);

            const int b = Push2Panel::paletteIndexForButton (bianco, true);
            const int v = Push2Panel::paletteIndexForButton (verde, true);
            const int r = Push2Panel::paletteIndexForButton (rosso, true);

            expect (b != v && v != r && b != r, "i colori dei tasti si pestano i piedi");

            // smorto e pieno sono due voci diverse
            expect (Push2Panel::paletteIndexForButton (verde, false) != v);

            // e tutto sta dentro la tavolozza del Push
            for (int index : { b, v, r })
                expect (index > 0 && index <= Push2Panel::numberOfPaletteEntries(),
                        "voce fuori dalla tavolozza: " + juce::String (index));

            // i colori dei tasti non devono pestare quelli dei suoni
            for (int i = 0; i < sound::numKinds; ++i)
            {
                const auto kind = sound::kindFromIndex (i);
                for (auto light : { machine::PadLight::loaded, machine::PadLight::selected,
                                    machine::PadLight::playing })
                    expect (Push2Panel::paletteIndexFor (light, kind) != v,
                            "un suono e un tasto usano la stessa voce");
            }
        }
    }
};

static TransportLightTests transportLightTests;
