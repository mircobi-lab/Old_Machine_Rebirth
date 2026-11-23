#include "machine/Machine.h"

#include <juce_core/juce_core.h>

using namespace machine;

class MachineTests final : public juce::UnitTest
{
public:
    MachineTests() : juce::UnitTest ("Macchina", "Macchina") {}

    static control::Control button (const char* label)
    {
        for (const auto& c : control::buttons())
            if (c.label == juce::String (label))
                return c;
        return {};
    }

    static void press (Machine& m, const char* label)
    {
        const auto c = button (label);
        m.command (c, true);
        m.command (c, false);
    }

    void runTest() override
    {
        beginTest ("appena accesa ha il kit di partenza e il primo pad scelto");
        {
            Machine m;
            expectEquals (m.engine().loadedCount(), 8);
            expect (m.selectedPad() == PadRef { 0, 1 });
            expect (m.lightFor ({ 0, 1 }) == PadLight::selected);
            expect (m.lightFor ({ 0, 2 }) == PadLight::loaded, "il rullante c'e'");
            expect (m.lightFor ({ 0, 16 }) == PadLight::empty, "questo e' vuoto");
        }

        beginTest ("battere un pad lo sceglie e lo fa suonare");
        {
            Machine m;
            m.prepare (44100.0);
            m.padDown ({ 0, 2 }, 100);

            juce::AudioBuffer<float> out (2, 512);
            m.process (out);

            expect (m.selectedPad() == PadRef { 0, 2 });
            expect (out.getMagnitude (0, 512) > 0.0f);
        }

        beginTest ("FULL LEVEL: la velocity non conta piu'");
        {
            auto peakWith = [] (bool fullLevel, int velocity)
            {
                Machine m;
                m.prepare (44100.0);
                if (fullLevel)
                    press (m, "FULL LEVEL");
                m.padDown ({ 0, 1 }, velocity);
                juce::AudioBuffer<float> out (2, 1024);
                m.process (out);
                return out.getMagnitude (0, 1024);
            };

            expect (peakWith (false, 30) < peakWith (false, 127) * 0.5f);
            expectWithinAbsoluteError (peakWith (true, 30), peakWith (true, 127), 0.001f);
        }

        beginTest ("MUTE: il pad mutato non suona e si vede rosso");
        {
            Machine m;
            m.prepare (44100.0);
            press (m, "MUTE");
            m.padDown ({ 0, 1 }, 127);          // in modo MUTE battere significa mutare
            expect (m.isMuted ({ 0, 1 }));
            expect (m.lightFor ({ 0, 1 }) == PadLight::muted);

            press (m, "MUTE");                   // si esce dal modo
            m.padDown ({ 0, 1 }, 127);
            juce::AudioBuffer<float> out (2, 1024);
            m.process (out);
            expectEquals (out.getMagnitude (0, 1024), 0.0f, "mutato non suona");
        }

        beginTest ("CANCELLA e ANNULLA");
        {
            Machine m;
            m.prepare (44100.0);
            expect (m.engine().drum (0).loaded());

            press (m, "CANCELLA");
            m.padDown ({ 0, 1 }, 100);
            expect (! m.engine().drum (0).loaded(), "il pad e' stato svuotato");

            press (m, "ANNULLA");
            expect (m.engine().drum (0).loaded(), "l'annulla rimette il suono");
        }

        beginTest ("16 LIVELLI: tutti i pad suonano il suono scelto, piano piano piu' forte");
        {
            auto peakOfPad = [] (int pad)
            {
                Machine m;
                m.prepare (44100.0);

                juce::AudioBuffer<float> scarto (2, 2048);
                m.padDown ({ 0, 1 }, 127);      // sceglie la cassa, e la suona
                m.process (scarto);             // qui la cassa parte davvero
                press (m, "STOP");              // la zittisce, cosi' non sporca la misura

                press (m, "16 LIVELLI");
                m.padDown ({ 0, pad }, 127);

                juce::AudioBuffer<float> out (2, 2048);
                m.process (out);
                return out.getMagnitude (0, 2048);
            };

            const float basso = peakOfPad (1);
            const float alto = peakOfPad (16);
            expect (alto > basso * 4.0f, "il pad 16 deve essere molto piu' forte del pad 1");
        }

        beginTest ("la colonna della quantizzazione cambia il passo");
        {
            Machine m;
            expectWithinAbsoluteError (quantiseBeats (7), 1.0, 0.0001);      // 1/4
            expectWithinAbsoluteError (quantiseBeats (3), 0.25, 0.0001);     // 1/16
            expectWithinAbsoluteError (quantiseBeats (4), 1.0 / 3.0, 0.0001);// 1/8 di terzina
        }

        beginTest ("i rotativi cambiano i parametri del suono scelto");
        {
            Machine m;

            // i parametri del suono stanno nella pagina SUONO: nella pagina
            // CAMPIONA i primi tre rotativi sono quelli del registratore
            press (m, "SUONO");

            auto turn = [&m] (int index, int steps)
            {
                m.encoder (control::forEncoder (71 + index), steps);
            };

            const float level = m.engine().drum (0).level;
            turn (0, 5);
            expect (m.engine().drum (0).level > level);

            turn (1, 3);
            expectEquals (m.engine().drum (0).tuneSemitones, 3);

            turn (6, 1);
            expect (m.engine().drum (0).reverse, "il settimo rotativo rovescia il suono");
        }

        beginTest ("il tempo si cambia e resta nei limiti");
        {
            Machine m;
            m.encoder (control::forEncoder (14), 20);
            expectWithinAbsoluteError (m.tempo(), 140.0, 0.001);
            m.encoder (control::forEncoder (14), -500);
            expectWithinAbsoluteError (m.tempo(), 30.0, 0.001);
        }

        beginTest ("i tasti si accendono quando il modo e' acceso");
        {
            Machine m;
            expect (! m.buttonLit (button ("FULL LEVEL")));
            press (m, "FULL LEVEL");
            expect (m.buttonLit (button ("FULL LEVEL")));
            expect (m.buttonLit (button ("CAMPIONA")), "la pagina aperta e' accesa");
        }
    }
};

static MachineTests machineTests;
