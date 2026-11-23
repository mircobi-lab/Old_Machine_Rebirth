#include "machine/Machine.h"
#include "push2/MachineScreen.h"
#include "push2/Waveform.h"

#include <juce_core/juce_core.h>

// L'onda sul display e lo step sequencer nella meta' di sopra della griglia.
class WaveStepTests final : public juce::UnitTest
{
public:
    WaveStepTests() : juce::UnitTest ("Onda e passi", "Macchina") {}

    static constexpr double rate = 44100.0;

    static juce::AudioBuffer<float> ramp (int length)
    {
        juce::AudioBuffer<float> b (2, length);
        for (int i = 0; i < length; ++i)
        {
            const float v = (float) i / (float) length;      // da 0 a 1
            b.setSample (0, i, v);
            b.setSample (1, i, -v);
        }
        return b;
    }

    static control::Control page (control::Command wanted)
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
        beginTest ("i picchi riassumono il campione colonna per colonna");
        {
            const auto view = mbstudio::computePeaks (ramp (10000), 100);
            expectEquals ((int) view.lows.size(), 100);
            expectEquals ((int) view.highs.size(), 100);

            // la rampa sale: l'ultima colonna ha il massimo piu' alto della prima
            expect (view.highs.back() > view.highs.front(), "i picchi non seguono il campione");
            expect (view.lows.back() < view.lows.front(), "il canale negativo non c'e'");
            expect (view.highs.back() > 0.9f && view.highs.back() <= 1.0f);
            expect (view.lows.front() <= 0.0f);
        }

        beginTest ("un campione vuoto non fa picchi e non fa danni");
        {
            expect (mbstudio::computePeaks ({}, 100).isEmpty());
            expect (mbstudio::computePeaks (juce::AudioBuffer<float> (2, 0), 100).isEmpty());

            // piu' colonne che campioni: ogni colonna prende almeno un campione
            const auto view = mbstudio::computePeaks (ramp (10), 100);
            expectEquals ((int) view.lows.size(), 100);
        }

        beginTest ("l'onda si vede dove si lavora sul campione");
        {
            machine::Machine m;
            m.prepare (rate);

            // il pad scelto ha il kit di partenza, quindi un campione c'e'
            press (m, page (control::Command::pageSound));
            expect (m.waveMarkers().show, "nella pagina SUONO l'onda non c'e'");

            press (m, page (control::Command::pageChop));
            expect (m.waveMarkers().show, "nella pagina FETTE l'onda non c'e'");

            press (m, page (control::Command::pageMix));
            expect (! m.waveMarkers().show, "l'onda c'e' anche dove non serve");
        }

        beginTest ("i tagli proposti seguono il numero delle fette");
        {
            machine::Machine m;
            m.prepare (rate);
            press (m, page (control::Command::pageChop));

            const auto before = m.waveMarkers();
            expectEquals ((int) before.slices.size(), m.sliceCount() - 1);
            expectWithinAbsoluteError (before.slices.front(), 1.0 / m.sliceCount(), 0.0001);

            // il primo rotativo cambia il numero: i tagli si muovono
            m.encoder (control::forEncoder (71), 1);
            const auto after = m.waveMarkers();
            expect (after.slices.size() > before.slices.size(),
                    "i tagli non sono cambiati col numero delle fette");
        }

        beginTest ("il ritaglio si vede come inizio e fine dell'onda");
        {
            machine::Machine m;
            m.prepare (rate);
            press (m, page (control::Command::pageSound));

            const auto before = m.waveMarkers();
            expectWithinAbsoluteError (before.startFraction, 0.0, 0.0001);
            expectWithinAbsoluteError (before.endFraction, 1.0, 0.0001);

            m.encoder (control::forEncoder (71 + 3), 10);     // INIZIO
            const auto after = m.waveMarkers();
            expect (after.startFraction > 0.0, "l'inizio non si e' mosso");
            expect (after.startFraction < after.endFraction);
        }

        beginTest ("la griglia dei passi: 32 celle nella meta' di sopra");
        {
            expect (machine::isStepCell (0));
            expect (machine::isStepCell (3));
            expect (! machine::isStepCell (4));
            expect (! machine::isStepCell (-1));

            expectEquals (machine::stepForCell (0, 0), 0);
            expectEquals (machine::stepForCell (0, 7), 7);
            expectEquals (machine::stepForCell (3, 7), 31);
            expectEquals (machine::stepForCell (4, 0), -1);

            for (int step = 0; step < machine::numSteps; ++step)
            {
                const auto cell = machine::cellForStep (step);
                expect (cell.has_value());
                expectEquals (machine::stepForCell (cell->first, cell->second), step);
            }

            expect (! machine::cellForStep (32).has_value());
        }

        beginTest ("nella pagina PASSO la meta' di sopra scrive, quella di sotto suona");
        {
            machine::Machine m;
            m.prepare (rate);
            press (m, page (control::Command::pageStep));
            expect (m.stepMode());

            // la cella in alto a sinistra: primo passo del suono scelto
            const auto stepPad = machine::padForCell (0, 0, m.bankGroup());
            expectEquals (m.sequencer().eventCount(), 0);

            m.padDown (stepPad, 100);
            m.padUp (stepPad);
            expectEquals (m.sequencer().eventCount(), 1, "il passo non e' stato scritto");
            expect (m.lightFor (stepPad) == machine::PadLight::loaded,
                    "il passo scritto non si accende");

            // premuto di nuovo si toglie
            m.padDown (stepPad, 100);
            m.padUp (stepPad);
            expectEquals (m.sequencer().eventCount(), 0, "il passo non si toglie");

            // la meta' di sotto resta pad: sceglie e suona, non scrive passi
            const auto soundPad = machine::padForCell (7, 0, m.bankGroup());
            const auto before = m.selectedPad();
            m.padDown (soundPad, 100);
            m.padUp (soundPad);
            expect (! (m.selectedPad() == before) || soundPad == before,
                    "il pad in basso non ha scelto il suono");
            expectEquals (m.sequencer().eventCount(), 0, "il pad in basso ha scritto un passo");
        }

        beginTest ("i passi scritti a mano suonano quando la sequenza gira");
        {
            machine::Machine m;
            m.prepare (rate);
            press (m, page (control::Command::pageStep));

            // quattro passi in croce sul primo suono
            for (int step : { 0, 4, 8, 12 })
            {
                const auto cell = machine::cellForStep (step);
                const auto pad = machine::padForCell (cell->first, cell->second, m.bankGroup());
                m.padDown (pad, 100);
                m.padUp (pad);
            }

            expectEquals (m.sequencer().eventCount(), 4);

            press (m, page (control::Command::pageSequence));
            control::Control play;
            for (const auto& c : control::buttons())
                if (c.command == control::Command::play)
                    play = c;
            press (m, play);

            // una battuta a 120: due secondi. Si conta quando esce suono.
            juce::AudioBuffer<float> buffer (2, 128);
            int hits = 0;
            bool quiet = true;

            for (int i = 0; i < (int) (rate * 2.0 / 128.0); ++i)
            {
                buffer.clear();
                m.process (buffer);
                const bool silent = buffer.getMagnitude (0, 128) < 0.01f;
                if (quiet && ! silent)
                    ++hits;
                quiet = silent;
            }

            expect (hits >= 3, "i passi scritti a mano non suonano: " + juce::String (hits));
        }

        beginTest ("la finestra dei passi si sposta avanti e indietro");
        {
            machine::Machine m;
            m.prepare (rate);
            press (m, page (control::Command::pageStep));
            m.sequencer().setBars (8);                  // ben piu' di 32 passi

            expectEquals (m.stepWindowStart(), 0);

            press (m, page (control::Command::pageRight));
            expectEquals (m.stepWindowStart(), machine::numSteps);

            press (m, page (control::Command::pageLeft));
            expectEquals (m.stepWindowStart(), 0);

            press (m, page (control::Command::pageLeft));
            expectEquals (m.stepWindowStart(), 0, "la finestra e' andata sotto zero");
        }

        beginTest ("l'ascolto fa suonare il campione e il segno corre sull'onda");
        {
            machine::Machine m;
            m.prepare (rate);
            press (m, page (control::Command::pageSound));

            // fermo, il segno non c'e'
            expect (m.waveMarkers().playFraction < 0.0, "il segno c'e' anche se non suona");

            m.auditionSelected();

            juce::AudioBuffer<float> buffer (2, 256);
            m.process (buffer);
            expect (buffer.getMagnitude (0, 256) > 0.01f, "l'ascolto non suona");

            const auto first = m.waveMarkers().playFraction;
            expect (first >= 0.0, "il segno non si vede mentre suona");

            for (int i = 0; i < 10; ++i)
            {
                buffer.clear();
                m.process (buffer);
            }

            expect (m.waveMarkers().playFraction > first, "il segno non si muove");

            // il tasto sotto il display fa la stessa cosa
            expectEquals (m.softKeyLabels()[0], juce::String ("ASCOLTA"));
        }

        beginTest ("l'ascolto non scrive niente nella sequenza");
        {
            machine::Machine m;
            m.prepare (rate);

            control::Control rec;
            for (const auto& c : control::buttons())
                if (c.command == control::Command::recordSequence)
                    rec = c;

            press (m, rec);
            expect (m.sequencer().isRecording());

            m.auditionSelected();
            expectEquals (m.sequencer().eventCount(), 0, "l'ascolto e' finito nella sequenza");
        }

        beginTest ("il display si disegna con l'onda e si puo' guardare");
        {
            machine::Machine m;
            m.prepare (rate);
            press (m, page (control::Command::pageChop));

            auto view = mbstudio::computePeaks (m.engine().copyOf (m.selectedDrumIndex()), 900);
            const auto markers = m.waveMarkers();
            view.startFraction = markers.startFraction;
            view.endFraction = markers.endFraction;
            view.slices = markers.slices;
            view.caption = markers.caption;

            const auto image = mbstudio::renderMachineScreen (m.screenLines(), "A", "D", nullptr, view);
            expectEquals (image.getWidth(), 960);
            expectEquals (image.getHeight(), 160);

            // non deve essere tutto nero: l'onda e le scritte ci sono
            int litPixels = 0;
            for (int y = 0; y < image.getHeight(); y += 2)
                for (int x = 0; x < image.getWidth(); x += 2)
                    if (image.getPixelAt (x, y).getPerceivedBrightness() > 0.2f)
                        ++litPixels;

            expect (litPixels > 500, "il display e' quasi vuoto: " + juce::String (litPixels));

            // e si salva, per poterlo guardare
            auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("display-fette.png");

            // il file va cancellato prima: JUCE apre in coda, e scrivendoci
            // sopra resterebbe quello vecchio con l'immagine nuova appiccicata
            // in fondo
            file.deleteFile();

            juce::PNGImageFormat png;
            std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
            if (stream != nullptr)
                png.writeImageToStream (image, *stream);

            logMessage ("display salvato in " + file.getFullPathName());
        }
    }
};

static WaveStepTests waveStepTests;
