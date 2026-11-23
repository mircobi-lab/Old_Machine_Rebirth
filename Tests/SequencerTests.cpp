#include "engine/Sequencer.h"
#include "machine/Machine.h"

#include <juce_core/juce_core.h>

// Il sequencer. Si fa girare il filo dell'audio a mano, un blocco alla volta,
// e si conta quante volte il campionatore viene chiamato.
class SequencerTests final : public juce::UnitTest
{
public:
    SequencerTests() : juce::UnitTest ("Sequencer", "Macchina") {}

    static constexpr double rate = 44100.0;

    // Un campionatore con un suono su ogni pad che serve, cosi' i colpi si
    // sentono davvero e si possono contare nel buffer.
    static void loadBeep (SampleEngine& engine, int index)
    {
        juce::AudioBuffer<float> b (1, 200);
        for (int i = 0; i < 200; ++i)
            b.setSample (0, i, 0.9f);
        engine.loadBuffer (index, "BEEP" + juce::String (index), b, rate);
    }

    // Fa girare il sequencer per tanti secondi e dice quanti colpi ha fatto
    // partire, guardando il suono che esce.
    static int runFor (Sequencer& seq, SampleEngine& engine, double seconds, double bpm)
    {
        const int blockSize = 128;
        const int blocks = (int) (seconds * rate / blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        int hits = 0;
        bool wasQuiet = true;

        for (int i = 0; i < blocks; ++i)
        {
            seq.process (blockSize, bpm, engine);
            buffer.clear();
            engine.process (buffer);

            const bool quiet = buffer.getMagnitude (0, blockSize) < 0.01f;
            if (wasQuiet && ! quiet)
                ++hits;
            wasQuiet = quiet;
        }

        return hits;
    }

    void runTest() override
    {
        beginTest ("appena accesa la sequenza e' vuota e ferma");
        {
            Sequencer seq;
            seq.prepare (rate);
            expect (! seq.isPlaying());
            expect (! seq.isRecording());
            expectEquals (seq.eventCount(), 0);
            expectEquals (seq.bars(), 2);
            expectEquals (seq.positionBar(), 1);
        }

        beginTest ("un colpo registrato torna a ogni giro");
        {
            Sequencer seq;
            SampleEngine engine;
            seq.prepare (rate);
            engine.prepare (rate);
            loadBeep (engine, 0);

            seq.setBars (1);                       // una battuta: a 120 sono due secondi
            seq.toggleRecord();
            expect (seq.isRecording() && seq.isPlaying());

            seq.recordHit (0, 100);
            expectEquals (seq.eventCount(), 1);

            // due giri da una battuta: il colpo deve tornare due volte
            const int hits = runFor (seq, engine, 4.0, 120.0);
            expect (hits >= 2 && hits <= 3, "colpi sentiti: " + juce::String (hits));
        }

        beginTest ("fermo non registra niente");
        {
            Sequencer seq;
            seq.prepare (rate);
            seq.recordHit (3, 100);
            expectEquals (seq.eventCount(), 0);
        }

        beginTest ("la correzione incolla il colpo al passo");
        {
            Sequencer seq;
            seq.prepare (rate);
            seq.setBars (1);
            seq.setTimingCorrect (true);
            seq.setQuantiseTicks (Sequencer::ticksPerBeat);     // un quarto
            seq.toggleRecord();

            // un colpo appena fuori tempo finisce sul quarto
            seq.recordHit (5, 100);
            expectEquals (seq.eventCount(), 1);

            seq.setTimingCorrect (false);
            seq.recordHit (5, 100);
            expectEquals (seq.eventCount(), 2);
        }

        beginTest ("cancellare un pad toglie solo i suoi colpi");
        {
            Sequencer seq;
            seq.prepare (rate);
            seq.toggleRecord();
            seq.recordHit (1, 100);
            seq.recordHit (2, 100);
            seq.recordHit (1, 100);
            expectEquals (seq.eventCount(), 3);

            seq.eraseDrum (1);
            expectEquals (seq.eventCount(), 1);
        }

        beginTest ("raddoppia e dimezza le battute");
        {
            Sequencer seq;
            seq.prepare (rate);
            seq.setBars (2);
            seq.toggleRecord();
            seq.recordHit (0, 100);
            seq.recordHit (4, 100);

            seq.doubleLength();
            expectEquals (seq.bars(), 4);
            expectEquals (seq.eventCount(), 4, "i colpi non sono stati copiati");

            seq.halveLength();
            expectEquals (seq.bars(), 2);
            expectEquals (seq.eventCount(), 2, "i colpi fuori non sono stati tolti");
        }

        beginTest ("le sequenze sono separate");
        {
            Sequencer seq;
            seq.prepare (rate);
            seq.toggleRecord();
            seq.recordHit (0, 100);
            expectEquals (seq.eventCount(), 1);

            seq.newSequence();
            expect (seq.sequenceIndex() != 0);
            expectEquals (seq.eventCount(), 0, "la sequenza nuova non e' vuota");
            expectEquals (seq.eventCount (0), 1, "la prima sequenza ha perso i colpi");
        }

        beginTest ("la copia porta colpi e battute nella sequenza vuota");
        {
            Sequencer seq;
            seq.prepare (rate);
            seq.setBars (4);
            seq.toggleRecord();
            seq.recordHit (7, 90);
            seq.recordHit (8, 90);

            seq.copyTo (5);
            seq.setSequence (5);
            expectEquals (seq.eventCount(), 2);
            expectEquals (seq.bars(), 4);
        }

        beginTest ("stop ferma, e premuto di nuovo torna all'inizio");
        {
            Sequencer seq;
            SampleEngine engine;
            seq.prepare (rate);
            engine.prepare (rate);
            loadBeep (engine, 0);

            seq.setBars (2);
            seq.play();
            runFor (seq, engine, 1.0, 120.0);
            expect (seq.positionInBeats() > 0.5, "non si e' mosso");

            seq.stop();
            expect (! seq.isPlaying());
            const auto where = seq.positionInBeats();
            expect (where > 0.5, "si e' azzerato troppo presto");

            seq.stop();
            expectWithinAbsoluteError (seq.positionInBeats(), 0.0, 0.001);
        }

        beginTest ("lo swing sposta i sedicesimi dispari ma non cambia i colpi");
        {
            Sequencer seq;
            seq.prepare (rate);
            seq.toggleRecord();
            seq.recordHit (0, 100);

            seq.setSwing (70);
            expectEquals (seq.swing(), 70);
            expectEquals (seq.eventCount(), 1);

            seq.setSwing (200);            // fuori scala: si ferma al massimo
            expectEquals (seq.swing(), 75);
        }

        beginTest ("la macchina non pesa sulla pila");
        {
            // Le 99 sequenze sono un paio di megabyte: devono stare nel
            // mucchio, altrimenti una macchina creata sulla pila la fa
            // scoppiare, e su Windows la pila e' un megabyte.
            expect (sizeof (Sequencer) < 64 * 1024,
                    "il sequencer pesa " + juce::String ((int) sizeof (Sequencer)) + " byte");
            expect (sizeof (machine::Machine) < 256 * 1024,
                    "la macchina pesa " + juce::String ((int) sizeof (machine::Machine)) + " byte");
        }

        beginTest ("i tasti del trasporto accendono le spie giuste");
        {
            machine::Machine m;
            m.prepare (rate);

            const auto find = [] (control::Command wanted)
            {
                for (const auto& c : control::buttons())
                    if (c.command == wanted)
                        return c;
                return control::Control {};
            };

            const auto play = find (control::Command::play);
            const auto rec = find (control::Command::recordSequence);

            m.command (play, true);
            m.command (play, false);
            expect (m.sequencer().isPlaying());
            expect (m.buttonLit (play));

            m.command (rec, true);
            m.command (rec, false);
            expect (m.sequencer().isRecording());
            expect (m.buttonLit (rec));

            const auto stop = find (control::Command::stop);
            m.command (stop, true);
            m.command (stop, false);
            expect (! m.sequencer().isPlaying());
            expect (! m.sequencer().isRecording());
        }

        beginTest ("battere sui pad mentre registra riempie la sequenza");
        {
            machine::Machine m;
            m.prepare (rate);

            const auto find = [] (control::Command wanted)
            {
                for (const auto& c : control::buttons())
                    if (c.command == wanted)
                        return c;
                return control::Control {};
            };

            const auto rec = find (control::Command::recordSequence);
            m.command (rec, true);
            m.command (rec, false);

            m.padDown ({ 0, 1 }, 100);
            m.padUp ({ 0, 1 });
            m.padDown ({ 0, 2 }, 110);
            m.padUp ({ 0, 2 });

            expectEquals (m.sequencer().eventCount(), 2);
        }

        beginTest ("le frecce spostano il pad scelto sulla griglia");
        {
            machine::Machine m;
            m.prepare (rate);

            const auto find = [] (control::Command wanted)
            {
                for (const auto& c : control::buttons())
                    if (c.command == wanted)
                        return c;
                return control::Control {};
            };

            const auto start = m.selectedPad();
            const auto up = find (control::Command::up);
            m.command (up, true);
            m.command (up, false);

            const auto after = m.selectedPad();
            expect (! (after == start), "la freccia non ha spostato niente");

            // e sul bordo non si esce dalla griglia
            for (int i = 0; i < 20; ++i)
            {
                m.command (up, true);
                m.command (up, false);
            }

            expect (m.selectedDrumIndex() >= 0 && m.selectedDrumIndex() < machine::numDrums);
        }
    }
};

static SequencerTests sequencerTests;
