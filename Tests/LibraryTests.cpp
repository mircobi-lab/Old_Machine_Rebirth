#include "machine/Library.h"

#include <juce_core/juce_core.h>

// Scene e libreria: si salva, si cambia tutto, si riapre e deve tornare come
// prima. Si lavora in una cartella temporanea, non in quella dell'utente.
class LibraryTests final : public juce::UnitTest
{
public:
    LibraryTests() : juce::UnitTest ("Scene e libreria", "Macchina") {}

    static juce::AudioBuffer<float> tone (double rate, double seconds, double hz)
    {
        const int length = (int) (rate * seconds);
        juce::AudioBuffer<float> b (2, length);
        for (int i = 0; i < length; ++i)
        {
            const auto v = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz
                                                        * (double) i / rate);
            b.setSample (0, i, v);
            b.setSample (1, i, v * 0.8f);
        }
        return b;
    }

    void runTest() override
    {
        const double rate = 44100.0;
        auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("rebirth-test-" + juce::String (juce::Random::getSystemRandom().nextInt (100000)));
        temp.createDirectory();

        beginTest ("una scena salvata e riaperta rimette tutto a posto");
        {
            machine::Machine m;
            m.prepare (rate);

            // una macchina con qualcosa dentro: un suono nostro su un pad
            // lontano, i parametri cambiati e un pad muto
            const machine::PadRef pad { 2, 7 };
            const int index = machine::drumIndex (pad);
            expect (m.engine().loadBuffer (index, "CASSA MIA", tone (rate, 0.4, 70.0), rate));

            auto& d = m.engine().drum (index);
            d.level = 0.42f;
            d.tuneSemitones = -5;
            d.pan = 0.3f;
            d.muteGroup = 2;
            d.reverse = true;

            auto state = m.snapshot();
            state.bpm = 93.0;
            state.quantise = 5;
            state.selectedPad = index;
            state.mutes[(size_t) index] = true;
            m.restore (state);

            const auto folder = temp.getChildFile ("Scena uno");
            juce::String error;
            expect (library::saveScene (folder, m, error), "non salva: " + error);
            expect (folder.getChildFile ("scena.txt").existsAsFile());

            // la macchina viene stravolta
            machine::Machine other;
            other.prepare (rate);
            auto wrong = other.snapshot();
            wrong.bpm = 170.0;
            other.restore (wrong);

            expect (library::loadScene (folder, other, error), "non apre: " + error);

            const auto& back = other.engine().drum (index);
            expect (back.loaded(), "il suono non e' tornato");
            expectEquals (back.name, juce::String ("CASSA MIA"));
            expectWithinAbsoluteError (back.level, 0.42f, 0.001f);
            expectEquals (back.tuneSemitones, -5);
            expectWithinAbsoluteError (back.pan, 0.3f, 0.001f);
            expectEquals (back.muteGroup, 2);
            expect (back.reverse);

            const auto restored = other.snapshot();
            expectWithinAbsoluteError (restored.bpm, 93.0, 0.01);
            expectEquals (restored.quantise, 5);
            expectEquals (restored.selectedPad, index);
            expect (restored.mutes[(size_t) index], "il muto non e' tornato");

            // e il suono e' quello di prima, non uno qualunque
            expect (back.sample.getNumSamples() > (int) (rate * 0.3));
            expect (back.sample.getMagnitude (0, back.sample.getNumSamples()) > 0.3f);
        }

        beginTest ("aprire una scena pulisce i pad che non ci sono dentro");
        {
            machine::Machine m;
            m.prepare (rate);
            const auto folder = temp.getChildFile ("Scena vuota");

            juce::String error;
            machine::Machine empty;
            empty.prepare (rate);
            empty.clearAllPads();
            expect (library::saveScene (folder, empty, error), error);

            // la macchina ha il kit di partenza: dopo l'apertura non deve
            // restare niente
            expect (m.engine().loadedCount() > 0);
            expect (library::loadScene (folder, m, error), error);
            expectEquals (m.engine().loadedCount(), 0);
        }

        beginTest ("una cartella che non e' una scena non fa danni");
        {
            machine::Machine m;
            m.prepare (rate);
            const int before = m.engine().loadedCount();

            juce::String error;
            expect (! library::loadScene (temp.getChildFile ("non esisto"), m, error));
            expect (error.isNotEmpty());
            expectEquals (m.engine().loadedCount(), before, "ha toccato i pad per niente");
        }

        beginTest ("la libreria tiene i campioni e non sovrascrive");
        {
            const auto folder = temp.getChildFile ("Libreria");
            const auto first = library::addToLibrary (folder, "RULLANTE", tone (rate, 0.2, 180.0), rate);
            expect (first.existsAsFile(), "non ha scritto il campione");
            expectEquals (first.getFileName(), juce::String ("RULLANTE.wav"));

            const auto second = library::addToLibrary (folder, "RULLANTE", tone (rate, 0.2, 200.0), rate);
            expect (second.existsAsFile());
            expect (second.getFileName() != first.getFileName(), "ha sovrascritto il primo");
            expectEquals (library::librarySize (folder), 2);

            // e il campione si rilegge sul pad
            machine::Machine m;
            m.prepare (rate);
            expect (m.engine().loadFile (0, second));
            expect (m.engine().drum (0).sample.getNumSamples() > (int) (rate * 0.1));
        }

        beginTest ("i nomi con caratteri strani non scappano dalla cartella");
        {
            const auto folder = temp.getChildFile ("Libreria");
            const auto file = library::addToLibrary (folder, "../../fuori", tone (rate, 0.1, 100.0), rate);
            expect (file == juce::File() || file.getParentDirectory() == folder,
                    "ha scritto fuori: " + file.getFullPathName());
        }

        temp.deleteRecursively();
    }
};

static LibraryTests libraryTests;
