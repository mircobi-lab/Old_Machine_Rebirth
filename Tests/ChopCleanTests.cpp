#include "engine/Chopper.h"
#include "engine/Cleaner.h"
#include "machine/Machine.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

using namespace machine;

// Le fette e la pulizia: il lavoro vero lo fanno i motori di Sample Forge e di
// AI Song Fixer, che hanno i loro collaudi nei rispettivi progetti. Qui si
// controlla che il collegamento regga: che un brano finto diventi fette sui
// pad, che le fette finiscano nei posti giusti e che la pulizia non sposti il
// campione nel tempo.
class ChopCleanTests final : public juce::UnitTest
{
public:
    ChopCleanTests() : juce::UnitTest ("Fette e pulizia", "Macchina") {}

    // Un brano finto: cassa ogni mezzo secondo, un rullante in mezzo, un basso
    // continuo e un po' di fruscio in alto.
    static juce::AudioBuffer<float> fakeSong (double rate, double seconds)
    {
        const int length = (int) (rate * seconds);
        juce::AudioBuffer<float> b (2, length);
        b.clear();

        juce::Random r (1234);
        const double beat = rate * 0.5;

        for (int i = 0; i < length; ++i)
        {
            const double t = (double) i / rate;
            float v = 0.2f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 55.0 * t);
            v += 0.05f * (r.nextFloat() * 2.0f - 1.0f);

            const double phase = std::fmod ((double) i, beat);
            if (phase < rate * 0.05)
            {
                const auto env = (float) (1.0 - phase / (rate * 0.05));
                v += env * 0.8f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * phase / rate);
            }

            b.setSample (0, i, v);
            b.setSample (1, i, v * 0.9f);
        }

        return b;
    }

    static juce::File writeTemp (const juce::AudioBuffer<float>& b, double rate)
    {
        auto file = juce::File::createTempFile (".wav");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.get(), rate, (unsigned int) b.getNumChannels(), 16, {}, 0));
        if (writer != nullptr)
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        return file;
    }

    void runTest() override
    {
        const double rate = 44100.0;

        beginTest ("un brano diventa fette con tempo e tonalita'");
        juce::File song;
        chopper::Result chopped;
        {
            song = writeTemp (fakeSong (rate, 12.0), rate);
            chopped = chopper::chop (song, 12);

            expect (chopped.error.isEmpty(), "errore: " + chopped.error);
            expect (! chopped.slices.empty(), "nessuna fetta");
            expect (chopped.bpm > 40.0 && chopped.bpm < 300.0, "tempo fuori strada: " + juce::String (chopped.bpm));

            for (const auto& s : chopped.slices)
            {
                expect (s.audio.getNumSamples() > 0, "fetta vuota");
                expect (s.name.isNotEmpty(), "fetta senza nome");
            }
        }

        beginTest ("un file che non e' audio non fa saltare niente");
        {
            auto bad = juce::File::createTempFile (".wav");
            bad.replaceWithText ("questo non e' un suono");
            const auto result = chopper::chop (bad, 10);
            expect (! result.ok());
            expect (result.error.isNotEmpty());
            bad.deleteFile();
        }

        beginTest ("le fette vanno sui pad, un banco per fila");
        {
            Machine m;
            m.prepare (rate);

            std::vector<chopper::Slice> slices;
            const auto makeSlice = [rate] (juce::String name, juce::String bank, sound::Kind kind)
            {
                chopper::Slice s;
                s.name = std::move (name);
                s.bank = std::move (bank);
                s.kind = kind;
                s.audio.setSize (1, 100);
                s.sampleRate = rate;
                return s;
            };

            for (int i = 0; i < 3; ++i)
                slices.push_back (makeSlice ("CASSA" + juce::String (i), "BATTERIA",
                                             sound::Kind::batteria));
            for (int i = 0; i < 2; ++i)
                slices.push_back (makeSlice ("VOCE" + juce::String (i), "VOCE",
                                             sound::Kind::voce));

            const int placed = m.placeSlices (slices);
            expectEquals (placed, 5);

            // tre di batteria sui primi tre pad, la voce ricomincia dal nono
            expectEquals (m.engine().drum (0).name, juce::String ("CASSA0"));
            expectEquals (m.engine().drum (2).name, juce::String ("CASSA2"));
            expect (! m.engine().drum (3).loaded() || m.engine().drum (3).name != "VOCE0");
            expectEquals (m.engine().drum (8).name, juce::String ("VOCE0"));
            expectEquals (m.engine().drum (9).name, juce::String ("VOCE1"));
        }

        beginTest ("la pulizia non sposta il campione nel tempo");
        {
            // un colpo secco a un decimo di secondo: dopo la pulizia deve stare
            // ancora li', perche' il ritardo del motore spettrale va tolto
            const int length = (int) rate;
            juce::AudioBuffer<float> hit (1, length);
            hit.clear();
            const int at = (int) (rate * 0.1);
            for (int i = 0; i < 500; ++i)
                hit.setSample (0, at + i, (float) std::sin (0.2 * i) * (1.0f - (float) i / 500.0f));

            juce::AudioBuffer<float> work (hit);
            expect (cleaner::clean (work, rate, cleaner::Preset::mp3));
            expectEquals (work.getNumSamples(), length);

            const auto peakOf = [] (const juce::AudioBuffer<float>& b)
            {
                int index = 0;
                float best = 0.0f;
                for (int i = 0; i < b.getNumSamples(); ++i)
                    if (std::abs (b.getSample (0, i)) > best)
                    {
                        best = std::abs (b.getSample (0, i));
                        index = i;
                    }
                return index;
            };

            const int before = peakOf (hit);
            const int after = peakOf (work);
            expect (std::abs (after - before) < (int) (rate * 0.01),
                    "il colpo si e' spostato di " + juce::String (after - before) + " campioni");
            expect (work.getMagnitude (0, length) > 0.05f, "la pulizia ha mangiato il suono");
        }

        beginTest ("il pad ripulito si puo' annullare");
        {
            Machine m;
            m.prepare (rate);
            const int index = m.selectedDrumIndex();
            const auto original = m.engine().copyOf (index);
            expect (original.getNumSamples() > 0, "il kit di partenza non c'e'");

            auto cleaned = original;
            cleaner::clean (cleaned, m.engine().drum (index).sourceRate, cleaner::Preset::ai);
            expect (m.applyCleanedSample (index, cleaned, m.engine().drum (index).sourceRate, "AI"));

            const auto undoButton = [&]
            {
                for (const auto& c : control::buttons())
                    if (c.command == control::Command::undo)
                        return c;
                return control::Control {};
            }();

            m.command (undoButton, true);
            m.command (undoButton, false);

            const auto back = m.engine().copyOf (index);
            expectEquals (back.getNumSamples(), original.getNumSamples());
            expectWithinAbsoluteError (back.getSample (0, 100), original.getSample (0, 100), 1.0e-6f);
        }

        beginTest ("i tasti sotto il display cambiano con la pagina");
        {
            Machine m;
            m.prepare (rate);

            const auto page = [] (control::Command wanted)
            {
                for (const auto& c : control::buttons())
                    if (c.command == wanted)
                        return c;
                return control::Control {};
            };

            auto chop = page (control::Command::pageChop);
            m.command (chop, true);
            m.command (chop, false);
            expectEquals (m.softKeyLabels()[0], juce::String ("SPACCA"));
            expectEquals (m.softKeyLabels()[1], juce::String ("PULISCI AI"));

            // la pagina CAMPIONA ha i suoi: quelli del registratore
            auto sample = page (control::Command::pageSample);
            m.command (sample, true);
            m.command (sample, false);
            expectEquals (m.softKeyLabels()[0], juce::String ("ARMA"));

            // una pagina che non ne ha ancora li lascia spenti
            auto mix = page (control::Command::pageMix);
            m.command (mix, true);
            m.command (mix, false);
            expect (m.softKeyLabels()[0].isEmpty());
        }

        song.deleteFile();
    }
};

static ChopCleanTests chopCleanTests;
