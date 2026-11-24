#include "engine/Compressor.h"
#include "engine/Warp.h"
#include "machine/Machine.h"

#include <juce_core/juce_core.h>

// Le funzioni prese dal pannello della macchina vera: avvio contato, brano,
// compressore, filtro sul pad, velocita' della sequenza, warp.
class AkaiFunctionTests final : public juce::UnitTest
{
public:
    AkaiFunctionTests() : juce::UnitTest ("Funzioni del pannello", "Macchina") {}

    static constexpr double rate = 44100.0;

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

    static juce::AudioBuffer<float> tone (double seconds, double hz, double amplitude = 0.5)
    {
        const int length = (int) (rate * seconds);
        juce::AudioBuffer<float> b (1, length);
        for (int i = 0; i < length; ++i)
            b.setSample (0, i, (float) (amplitude * std::sin (2.0 * juce::MathConstants<double>::pi
                                                                  * hz * i / rate)));
        return b;
    }

    // Quante volte il suono passa per lo zero: con la stessa nota il conto per
    // secondo resta lo stesso, anche se la durata cambia.
    static double zeroCrossingsPerSecond (const juce::AudioBuffer<float>& b)
    {
        const int length = b.getNumSamples();
        if (length < 2)
            return 0.0;

        int crossings = 0;
        for (int i = 1; i < length; ++i)
            if ((b.getSample (0, i - 1) <= 0.0f) != (b.getSample (0, i) <= 0.0f))
                ++crossings;

        return crossings / ((double) length / rate);
    }

    void runTest() override
    {
        beginTest ("l'avvio contato fa battere il metronomo prima di partire");
        {
            Sequencer seq;
            SampleEngine engine;
            seq.prepare (rate);
            engine.prepare (rate);

            seq.setCountInBars (1);
            seq.setBars (1);
            seq.toggleRecord();
            expect (seq.isCountingIn(), "non sta contando");

            // mezzo secondo: a 120 una battuta sono due secondi, quindi conta ancora
            for (int i = 0; i < (int) (rate * 0.5 / 256); ++i)
                seq.process (256, 120.0, engine);

            expect (seq.isCountingIn(), "ha smesso di contare troppo presto");
            expectWithinAbsoluteError (seq.positionInBeats(), 0.0, 0.01);

            // oltre la battuta: parte
            for (int i = 0; i < (int) (rate * 2.0 / 256); ++i)
                seq.process (256, 120.0, engine);

            expect (! seq.isCountingIn(), "conta ancora dopo la battuta");
            expect (seq.positionInBeats() > 0.0, "non e' partito");
        }

        beginTest ("il brano incatena le sequenze e gira");
        {
            Sequencer seq;
            SampleEngine engine;
            seq.prepare (rate);
            engine.prepare (rate);
            seq.setBars (1);

            seq.addToSong (0, 1);
            seq.addToSong (3, 1);
            expectEquals (seq.songLength(), 2);
            expectEquals (seq.songStep (1).sequence, 3);

            seq.setSongMode (true);
            expect (seq.songMode());
            expectEquals (seq.songPosition(), 0);

            seq.play();

            // una battuta a 120 sono due secondi: dopo deve essere passato al
            // secondo pezzo del brano
            for (int i = 0; i < (int) (rate * 2.2 / 256); ++i)
                seq.process (256, 120.0, engine);

            expectEquals (seq.songPosition(), 1, "il brano non e' andato avanti");
            expectEquals (seq.sequenceIndex(), 3, "non ha cambiato sequenza");

            seq.removeLastSongStep();
            expectEquals (seq.songLength(), 1);

            seq.clearSong();
            expectEquals (seq.songLength(), 0);
            seq.setSongMode (true);
            expect (! seq.songMode(), "il brano vuoto non deve accendersi");
        }

        beginTest ("meta' e doppia velocita'");
        {
            Sequencer seq;
            SampleEngine engine;
            seq.prepare (rate);
            engine.prepare (rate);
            seq.setBars (1);
            seq.play();

            // un secondo a velocita' normale
            for (int i = 0; i < (int) (rate / 256); ++i)
                seq.process (256, 120.0, engine);
            const double normale = seq.positionInBeats();

            seq.rewind();
            seq.setSpeed (2.0);
            for (int i = 0; i < (int) (rate / 256); ++i)
                seq.process (256, 120.0, engine);

            expect (seq.positionInBeats() > normale * 1.8,
                    "a doppia velocita' non corre il doppio");
        }

        beginTest ("il compressore tira giu' solo quello che supera la soglia");
        {
            Compressor comp;
            comp.prepare (rate);
            comp.setThresholdDb (-12.0);
            comp.setRatio (4.0);
            comp.setAttackMs (1.0);
            comp.setReleaseMs (50.0);

            // un suono piano: non lo deve toccare quasi
            auto quiet = tone (0.5, 220.0, 0.05);
            const float quietBefore = quiet.getMagnitude (0, quiet.getNumSamples());
            comp.process (quiet);
            expect (comp.gainReductionDb() < 1.0f,
                    "schiaccia anche il piano: " + juce::String (comp.gainReductionDb()));

            // un suono forte: lo deve tirare giu'
            auto loud = tone (0.5, 220.0, 0.9);
            comp.process (loud);
            expect (comp.gainReductionDb() > 3.0f,
                    "non schiaccia il forte: " + juce::String (comp.gainReductionDb()));

            // e non deve sfondare: niente numeri oltre il fondo scala
            expect (loud.getMagnitude (0, loud.getNumSamples()) < 1.5f, "esce distorto");
            juce::ignoreUnused (quietBefore);
        }

        beginTest ("il filtro del pad toglie gli acuti o i bassi");
        {
            machine::Machine m;
            m.prepare (rate);

            // un suono con dentro acuti: rumore
            juce::Random r (7);
            juce::AudioBuffer<float> noise (1, (int) (rate * 0.3));
            for (int i = 0; i < noise.getNumSamples(); ++i)
                noise.setSample (0, i, r.nextFloat() * 2.0f - 1.0f);

            const int index = m.selectedDrumIndex();
            m.engine().loadBuffer (index, "RUMORE", noise, rate);

            const auto render = [&m] (float filter)
            {
                m.engine().drum (m.selectedDrumIndex()).filter = filter;
                m.engine().stopAll();
                m.engine().trigger (m.selectedDrumIndex(), 1.0f);

                juce::AudioBuffer<float> out (2, 4096);
                out.clear();
                m.process (out);
                return zeroCrossingsPerSecond (out);
            };

            const double dritto = render (0.0f);
            const double scuro = render (1.0f);        // tutto a destra: via gli acuti

            expect (scuro < dritto * 0.8,
                    "il filtro non toglie gli acuti: " + juce::String (scuro)
                        + " contro " + juce::String (dritto));
        }

        beginTest ("il warp cambia la durata ma non il tono");
        {
            const auto source = tone (2.0, 220.0);
            const auto stretched = warp::stretch (source, 1.5);

            expect (stretched.getNumSamples() > source.getNumSamples() * 1.4,
                    "non si e' allungato");
            expect (stretched.getNumSamples() < source.getNumSamples() * 1.6,
                    "si e' allungato troppo");

            const double before = zeroCrossingsPerSecond (source);
            const double after = zeroCrossingsPerSecond (stretched);

            // stessa nota: lo stesso numero di passaggi per lo zero al secondo
            expect (std::abs (after - before) < before * 0.15,
                    "il tono e' cambiato: " + juce::String (before) + " contro "
                        + juce::String (after));
        }

        beginTest ("il warp mette il campione a tempo");
        {
            // un pezzo da un secondo e mezzo: a 120 una battuta sono due
            // secondi, quindi deve diventare lungo due
            int bars = 0;
            const double factor = warp::factorForTempo ((int) (rate * 1.5), rate, 120.0, bars);

            expectEquals (bars, 1);
            expectWithinAbsoluteError (factor, 2.0 / 1.5, 0.01);

            machine::Machine m;
            m.prepare (rate);
            const int index = m.selectedDrumIndex();
            m.engine().loadBuffer (index, "GIRO", tone (1.5, 110.0), rate);

            const int before = m.engine().drum (index).sample.getNumSamples();
            m.warpSelected();
            const int after = m.engine().drum (index).sample.getNumSamples();

            expect (after > before, "il warp non ha allungato il campione");
            expectWithinAbsoluteError ((double) after / rate, 2.0, 0.15);
        }

        beginTest ("i tasti nuovi stanno nelle loro pagine");
        {
            machine::Machine m;
            m.prepare (rate);

            press (m, find (control::Command::pageMix));
            expectEquals (m.softKeyLabels()[0], juce::String ("COMPRESSORE"));

            // il compressore si accende e si spegne
            expect (! m.compressorOn());
            m.command (control::forButton (20), true);
            m.command (control::forButton (20), false);
            expect (m.compressorOn(), "il compressore non si accende");

            press (m, find (control::Command::pageSong));
            expectEquals (m.softKeyLabels()[0], juce::String ("AGGIUNGI"));

            // SHIFT con MUTE fa suonare tutti
            m.command (find (control::Command::padMute), true);
            m.command (find (control::Command::padMute), false);
            expect (m.isMuteMode());

            const auto shift = find (control::Command::shift);
            m.command (shift, true);
            press (m, find (control::Command::padMute));
            m.command (shift, false);
        }
    }
};

static AkaiFunctionTests akaiFunctionTests;
