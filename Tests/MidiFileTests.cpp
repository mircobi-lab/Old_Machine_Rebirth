#include "machine/MidiFileIo.h"

#include <juce_core/juce_core.h>

// I file MIDI: dentro e fuori. Le note vanno sui pad col conto delle macchine
// vere, la 35 sul primo pad.
class MidiFileTests final : public juce::UnitTest
{
public:
    MidiFileTests() : juce::UnitTest ("File MIDI", "Macchina") {}

    void runTest() override
    {
        auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("rebirth-midi-"
                                           + juce::String (juce::Random::getSystemRandom().nextInt (100000)));
        temp.createDirectory();

        beginTest ("le note vanno sui pad come sulla macchina vera");
        {
            expectEquals (midifile::drumForNote (35), 0);
            expectEquals (midifile::drumForNote (36), 1);
            expectEquals (midifile::drumForNote (98), 63);
            expectEquals (midifile::drumForNote (34), -1, "sotto il primo pad non c'e' niente");
            expectEquals (midifile::drumForNote (200), -1);

            for (int drum = 0; drum < 64; ++drum)
                expectEquals (midifile::drumForNote (midifile::noteForDrum (drum)), drum);
        }

        beginTest ("un file MIDI entra nella sequenza");
        {
            // quattro casse in croce piu' due charleston, a 100 battute
            juce::MidiMessageSequence track;
            track.addEvent (juce::MidiMessage::tempoMetaEvent (600000));     // 100 battute

            // i tempi di un file MIDI si contano in passi: qui 96 per quarto
            const double step = 96.0;

            for (int beat = 0; beat < 4; ++beat)
            {
                track.addEvent (juce::MidiMessage::noteOn (10, 35, (juce::uint8) 110),
                                beat * step);
                track.addEvent (juce::MidiMessage::noteOff (10, 35), beat * step + 20.0);
            }

            track.addEvent (juce::MidiMessage::noteOn (10, 38, (juce::uint8) 80), step * 0.5);
            track.addEvent (juce::MidiMessage::noteOff (10, 38), step * 0.7);
            track.addEvent (juce::MidiMessage::noteOn (10, 38, (juce::uint8) 80), step * 2.5);
            track.addEvent (juce::MidiMessage::noteOff (10, 38), step * 2.7);

            juce::MidiFile midi;
            midi.setTicksPerQuarterNote (96);
            midi.addTrack (track);

            const auto file = temp.getChildFile ("giro.mid");
            {
                juce::FileOutputStream stream (file);
                expect (stream.openedOk());
                midi.writeTo (stream);
            }

            machine::Machine m;
            m.prepare (44100.0);

            const auto result = midifile::import (file, m);

            expect (result.ok(), "non ha caricato: " + result.error);
            expectEquals (result.notes, 6);
            expectWithinAbsoluteError (result.bpm, 100.0, 1.0);
            expectEquals (result.bars, 1, "una battuta basta per quattro quarti");
            expectEquals (m.sequencer().eventCount(), 6);
            expectWithinAbsoluteError (m.tempo(), 100.0, 1.0);

            // la cassa sul primo pad, il rullante sul quarto
            const int window = 4;
            expect (m.sequencer().hasHit (0, 0, window), "la cassa non e' sul primo quarto");
            expect (m.sequencer().hasHit (0, Sequencer::ticksPerBeat * 2, window));
            expect (m.sequencer().hasHit (3, Sequencer::ticksPerBeat / 2, window),
                    "il rullante non e' a meta' del primo quarto");
            expect (! m.sequencer().hasHit (3, 0, window), "c'e' un rullante di troppo");
        }

        beginTest ("un file senza note sui pad lo dice invece di far finta");
        {
            juce::MidiMessageSequence track;
            track.addEvent (juce::MidiMessage::noteOn (1, 20, (juce::uint8) 100), 0.0);

            juce::MidiFile midi;
            midi.setTicksPerQuarterNote (96);
            midi.addTrack (track);

            const auto file = temp.getChildFile ("fuori.mid");
            {
                juce::FileOutputStream stream (file);
                midi.writeTo (stream);
            }

            machine::Machine m;
            m.prepare (44100.0);

            const auto result = midifile::import (file, m);
            expect (! result.ok());
            expectEquals (result.error, juce::String ("NESSUNA NOTA SUI PAD"));
        }

        beginTest ("un file che non e' MIDI non fa danni");
        {
            auto bad = temp.getChildFile ("rotto.mid");
            bad.replaceWithText ("non sono un file midi");

            machine::Machine m;
            m.prepare (44100.0);

            const auto result = midifile::import (bad, m);
            expect (! result.ok());
            expect (result.error.isNotEmpty());
        }

        beginTest ("la sequenza esce in un file MIDI e rientra uguale");
        {
            machine::Machine m;
            m.prepare (44100.0);
            m.setTempoFromOutside (96.0);

            auto& seq = m.sequencer();
            seq.setBars (2);
            seq.clearSequence();
            seq.setHit (0, 0, 120);
            seq.setHit (5, Sequencer::ticksPerBeat, 90);
            seq.setHit (9, Sequencer::ticksPerBar + Sequencer::ticksPerBeat / 2, 70);

            const auto file = temp.getChildFile ("uscita.mid");
            expect (midifile::exportSequence (file, m), "non ha scritto il file");
            expect (file.getSize() > 0);

            // si rilegge con una macchina pulita: deve tornare tutto
            machine::Machine other;
            other.prepare (44100.0);

            const auto result = midifile::import (file, other);
            expect (result.ok(), result.error);
            expectEquals (result.notes, 3);
            expectWithinAbsoluteError (other.tempo(), 96.0, 1.0);

            const int window = 4;
            expect (other.sequencer().hasHit (0, 0, window), "la prima nota non e' tornata");
            expect (other.sequencer().hasHit (5, Sequencer::ticksPerBeat, window));
            expect (other.sequencer().hasHit (9, Sequencer::ticksPerBar + Sequencer::ticksPerBeat / 2,
                                              window), "la nota della seconda battuta non e' tornata");
            expectEquals (other.sequencer().bars(), 2, "le battute non tornano");
        }

        beginTest ("una sequenza vuota non scrive un file a vuoto");
        {
            machine::Machine m;
            m.prepare (44100.0);
            m.sequencer().clearSequence();

            expect (! midifile::exportSequence (temp.getChildFile ("vuota.mid"), m));
        }

        temp.deleteRecursively();
    }
};

static MidiFileTests midiFileTests;
