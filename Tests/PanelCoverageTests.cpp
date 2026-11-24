#include "machine/Machine.h"

#include <juce_core/juce_core.h>

// Quanti tasti del Push fanno qualcosa.
//
// Si premono tutti, uno per uno, e si guarda se la macchina cambia: il
// messaggio sul display, la pagina, il trasporto, il pad scelto. Quelli che
// non muovono niente vengono elencati, cosi' quando se ne aggiunge uno il
// conto scende da solo e non si resta con tasti morti senza saperlo.
class PanelCoverageTests final : public juce::UnitTest
{
public:
    PanelCoverageTests() : juce::UnitTest ("Tasti del pannello", "Macchina") {}

    // Come sta la macchina adesso, in una riga: se dopo il tasto questa riga
    // cambia, il tasto fa qualcosa.
    // Come sta la macchina adesso, in una riga: se dopo il tasto questa riga
    // cambia, il tasto fa qualcosa. Il messaggio sul display resta fuori,
    // perche' lo scrive qualunque tasto, anche uno che poi non combina niente.
    static juce::String fingerprint (machine::Machine& m)
    {
        auto lines = m.screenLines();
        lines.remove (lines.size() - 1);

        const auto& d = m.engine().drum (m.selectedDrumIndex());
        auto& seq = m.sequencer();
        auto& rec = m.recorder();

        int muted = 0;
        for (int i = 0; i < machine::numDrums; ++i)
            if (m.isMuted (machine::drumAt (i)))
                ++muted;

        juce::String s;
        s << lines.joinIntoString ("/") << "|"
          << m.selectedDrumIndex() << "|" << m.bankGroup() << "|" << muted << "|"
          << (int) m.isFullLevel() << (int) m.isSixteenLevels() << (int) m.isMuteMode() << "|"
          << (int) seq.isPlaying() << (int) seq.isRecording() << (int) seq.isOverdub() << "|"
          << seq.sequenceIndex() << "|" << seq.bars() << "|" << seq.eventCount() << "|"
          << (int) seq.timingCorrect() << "|" << seq.quantiseTicks() << "|" << seq.swing() << "|"
          << juce::String (seq.positionInBeats(), 3) << "|"
          << juce::String (seq.speed(), 2) << "|" << seq.countInBars() << "|"
          << seq.songLength() << (int) seq.songMode() << "|"
          << (int) m.compressorOn() << "|"
          << juce::String (m.compressor().thresholdDb(), 1) << "|"
          << juce::String (m.compressor().ratio(), 1) << "|"
          << juce::String (m.compressor().attackMs(), 1) << "|"
          << juce::String (m.compressor().releaseMs(), 1) << "|"
          << m.engine().activeVoices() << "|"
          << juce::String (m.tempo(), 2) << "|" << juce::String (m.masterVolume(), 3) << "|"
          << (int) m.padsRecord() << "|"
          << rec.threshold() << "|" << (int) rec.state() << "|"
          << juce::String (rec.lengthSeconds(), 2) << "|"
          << juce::String (rec.preRollSeconds(), 3) << "|" << (int) rec.monitoring() << "|"
          << m.sliceCount() << "|" << m.softKeyLabels().joinIntoString (",") << "|"
          << d.name << "|" << juce::String (d.level, 3) << "|" << d.tuneSemitones << "|"
          << juce::String (d.pan, 3) << "|" << d.start << "|" << d.end << "|"
          << d.muteGroup << "|" << (int) d.reverse << "|" << sound::indexOf (d.kind) << "|"
          << juce::String (d.filter, 2) << "|"
          << (int) d.playMode << "|"
          << m.engine().loadedCount() << "|";

        // le spie dei tasti: ci stanno dentro i modi che non si vedono
        // altrove, tipo CANCELLA o il metronomo
        for (const auto& b : control::buttons())
            s << (m.buttonLit (b) ? "1" : "0");

        return s;
    }

    // Due punti di partenza: la macchina appena accesa, e una macchina in
    // mezzo al lavoro. Alcuni tasti si vedono solo nella seconda: STOP ferma
    // se gira, ANNULLA serve se c'e' qualcosa da annullare, le frecce si
    // muovono se il pad scelto non e' in un angolo.
    static void makeBusy (machine::Machine& m)
    {
        const auto find = [] (control::Command wanted)
        {
            for (const auto& c : control::buttons())
                if (c.command == wanted)
                    return c;
            return control::Control {};
        };

        const auto press = [&m] (const control::Control& c)
        {
            m.command (c, true);
            m.command (c, false);
        };

        press (find (control::Command::recordSequence));     // gira e registra

        // un pad in mezzo alla meta' di sotto, cosi' le frecce hanno spazio
        // da tutte le parti
        const auto middle = machine::padForCell (5, 2, m.bankGroup());
        m.padDown (middle, 100);
        m.padUp (middle);

        const auto other = machine::padForCell (6, 3, m.bankGroup());
        m.padDown (other, 90);
        m.padUp (other);

        juce::AudioBuffer<float> buffer (2, 256);
        m.process (buffer);                                  // il tempo si muove

        // il passo non e' quello di partenza, cosi' anche i tasti della
        // colonna di destra hanno qualcosa da cambiare
        for (const auto& c : control::buttons())
            // non il primo ne' l'ultimo: quelli scorrono le tracce
            if (c.command == control::Command::quantiseValue && c.index == 6)
                press (c);

        // una fetta messa su un pad della coppia che si vede: cosi' ANNULLA
        // ha qualcosa da annullare, e il pad scelto resta sulla griglia
        chopper::Slice slice;
        slice.name = "PROVA";
        slice.bank = "FETTE";
        slice.audio.setSize (1, 500);
        slice.sampleRate = 44100.0;
        m.placeFrom ({ 1, 1 }, { slice });

        // e si torna in mezzo, cosi' le frecce hanno spazio da tutte le parti
        m.padDown (middle, 100);
        m.padUp (middle);
    }

    void runTest() override
    {
        beginTest ("tutti i tasti del pannello fanno qualcosa");
        {
            // Questi non cambiano lo stato della macchina per mestiere: SHIFT
            // lavora sugli altri, SCEGLI e SUONA dicono come trattare il
            // prossimo colpo, SETUP, SFOGLIA e A LIVE passano la palla alla
            // finestra, AGG EFFETTO aspetta gli effetti che non ci sono.
            const std::vector<control::Command> expected {
                control::Command::shift,
                control::Command::select,
                control::Command::setup,
                control::Command::browse,
                control::Command::handBackToLive,
                control::Command::addEffect,
                control::Command::tapTempo      // ha un collaudo suo: serve il tempo vero
            };

            const auto allowed = [&expected] (control::Command c)
            {
                for (auto e : expected)
                    if (e == c)
                        return true;
                return false;
            };

            // I tasti sotto il display cambiano con la pagina: si provano in
            // tutte, e basta che facciano qualcosa in una.
            const std::vector<control::Command> pages {
                control::Command::pageSample, control::Command::pageChop,
                control::Command::pageSound, control::Command::pageSequence,
                control::Command::pageStep, control::Command::pageSong,
                control::Command::pageMix, control::Command::pageDisk
            };

            juce::StringArray dead;
            int alive = 0;

            for (const auto& c : control::buttons())
            {
                if (allowed (c.command))
                    continue;

                bool moved = false;

                for (int busy = 0; busy < 2 && ! moved; ++busy)
                    for (auto pageCommand : pages)
                    {
                        machine::Machine m;
                        m.prepare (44100.0);

                        if (busy == 1)
                            makeBusy (m);

                        for (const auto& p : control::buttons())
                            if (p.command == pageCommand)
                            {
                                m.command (p, true);
                                m.command (p, false);
                            }

                        const auto before = fingerprint (m);

                        // si guarda anche col tasto giu': il NOTE REPEAT vale
                        // solo mentre lo si tiene premuto
                        m.command (c, true);
                        const bool movedWhileDown = fingerprint (m) != before;

                        m.command (c, false);

                        if (movedWhileDown || fingerprint (m) != before)
                        {
                            moved = true;
                            break;
                        }

                        // lo STOP premuto due volte torna all'inizio
                        m.command (c, true);
                        m.command (c, false);

                        if (fingerprint (m) != before)
                        {
                            moved = true;
                            break;
                        }
                    }

                if (moved)
                    ++alive;
                else
                    dead.add (c.label + " (" + juce::String (c.number) + ")");
            }

            logMessage ("tasti che cambiano la macchina: " + juce::String (alive));

            if (! dead.isEmpty())
                logMessage ("tasti ancora muti: " + dead.joinIntoString (", "));

            expect (dead.isEmpty(), "tasti muti: " + dead.joinIntoString (", "));
        }

        beginTest ("il TAP prende il tempo dalle battute");
        {
            machine::Machine m;
            m.prepare (44100.0);

            control::Control tap;
            for (const auto& c : control::buttons())
                if (c.command == control::Command::tapTempo)
                    tap = c;

            const auto before = m.tempo();

            // tre battute a 400 millesimi: fanno 150
            for (int i = 0; i < 3; ++i)
            {
                m.command (tap, true);
                m.command (tap, false);
                juce::Thread::sleep (400);
            }

            expect (std::abs (m.tempo() - before) > 1.0,
                    "il tempo non si e' mosso: " + juce::String (m.tempo()));
            expect (m.tempo() > 120.0 && m.tempo() < 180.0,
                    "tempo fuori strada: " + juce::String (m.tempo()));
        }

        beginTest ("i rotativi muovono tutti qualcosa");
        {
            juce::StringArray dead;

            for (const auto& e : control::encoders())
            {
                machine::Machine m;
                m.prepare (44100.0);

                const auto before = fingerprint (m);
                m.encoder (e, 3);

                if (fingerprint (m) == before)
                    dead.add (e.label);
            }

            expect (dead.isEmpty(), "rotativi muti: " + dead.joinIntoString (", "));
        }

        beginTest ("i rotativi della pagina SUONO muovono i parametri del campione");
        {
            machine::Machine m;
            m.prepare (44100.0);

            for (const auto& c : control::buttons())
                if (c.command == control::Command::pageSound)
                {
                    m.command (c, true);
                    m.command (c, false);
                }

            juce::StringArray dead;
            for (const auto& e : control::encoders())
            {
                const auto before = fingerprint (m);
                m.encoder (e, 2);
                if (fingerprint (m) == before)
                    dead.add (e.label);
            }

            expect (dead.isEmpty(), "rotativi muti nella pagina SUONO: " + dead.joinIntoString (", "));
        }

        beginTest ("la colonna di destra sceglie il passo e lo passa al sequencer");
        {
            machine::Machine m;
            m.prepare (44100.0);

            // il primo e l'ultimo scorrono le tracce; gli altri sei scelgono
            // il passo e si accendono quando sono quelli scelti
            for (const auto& c : control::buttons())
                if (c.command == control::Command::quantiseValue
                        && c.index > 0 && c.index < 7)
                {
                    m.command (c, true);
                    m.command (c, false);
                    expect (m.buttonLit (c), "il tasto del passo non si accende: " + c.label);
                }

            expect (m.sequencer().quantiseTicks() > 0);
        }
    }
};

static PanelCoverageTests panelCoverageTests;
