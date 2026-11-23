#include "machine/Library.h"
#include "machine/Machine.h"

#include <juce_core/juce_core.h>

// I quattro modi di suonare un pad, come sulla macchina vera: una volta,
// tenuto, a giro, avvia e ferma.
class PlayModeTests final : public juce::UnitTest
{
public:
    PlayModeTests() : juce::UnitTest ("Modi dei pad", "Macchina") {}

    static constexpr double rate = 44100.0;

    // Mezzo secondo di suono pieno su un pad.
    static void loadTone (SampleEngine& engine, int index, double seconds = 0.5)
    {
        const int length = (int) (rate * seconds);
        juce::AudioBuffer<float> b (1, length);
        for (int i = 0; i < length; ++i)
            b.setSample (0, i, 0.8f * (float) std::sin (0.05 * i));

        engine.loadBuffer (index, "PROVA", b, rate);
    }

    // Fa girare il campionatore e dice se alla fine stava ancora suonando.
    static bool soundingAfter (SampleEngine& engine, double seconds)
    {
        const int blockSize = 256;
        juce::AudioBuffer<float> buffer (2, blockSize);

        for (int i = 0; i < (int) (seconds * rate / blockSize); ++i)
        {
            buffer.clear();
            engine.process (buffer);
        }

        buffer.clear();
        engine.process (buffer);
        return buffer.getMagnitude (0, blockSize) > 0.01f;
    }

    void runTest() override
    {
        beginTest ("una volta: arriva in fondo anche se si molla subito");
        {
            SampleEngine engine;
            engine.prepare (rate);
            loadTone (engine, 0);
            engine.drum (0).playMode = PlayMode::unaVolta;

            engine.trigger (0, 1.0f);
            engine.release (0);                       // mollato subito
            expect (soundingAfter (engine, 0.1), "si e' fermato quando ho mollato");
            expect (! soundingAfter (engine, 0.6), "non e' finito da solo");
        }

        beginTest ("tenuto: si ferma quando si molla il pad");
        {
            SampleEngine engine;
            engine.prepare (rate);
            loadTone (engine, 0);
            engine.drum (0).playMode = PlayMode::tenuto;

            engine.trigger (0, 1.0f);
            expect (soundingAfter (engine, 0.1), "non parte");

            engine.release (0);
            expect (! soundingAfter (engine, 0.05), "non si ferma quando mollo");
        }

        beginTest ("giro: ricomincia da solo e si ferma al secondo colpo");
        {
            SampleEngine engine;
            engine.prepare (rate);
            loadTone (engine, 0, 0.2);
            engine.drum (0).playMode = PlayMode::giro;

            engine.trigger (0, 1.0f);
            engine.release (0);

            // ben oltre la fine del campione: deve essere ripartito
            expect (soundingAfter (engine, 0.9), "il giro si e' fermato da solo");

            engine.trigger (0, 1.0f);                 // secondo colpo: ferma
            expect (! soundingAfter (engine, 0.05), "il secondo colpo non ferma il giro");
        }

        beginTest ("avvia e ferma: il secondo colpo spegne");
        {
            SampleEngine engine;
            engine.prepare (rate);
            loadTone (engine, 0);
            engine.drum (0).playMode = PlayMode::avviaFerma;

            engine.trigger (0, 1.0f);
            expect (soundingAfter (engine, 0.1));

            engine.trigger (0, 1.0f);
            expect (! soundingAfter (engine, 0.05), "il secondo colpo non ferma");

            // e non gira: lasciato in pace finisce
            engine.trigger (0, 1.0f);
            expect (! soundingAfter (engine, 0.6), "avvia e ferma si e' messo a girare");
        }

        beginTest ("il rotativo gira fra i quattro modi e si ferma ai capi");
        {
            machine::Machine m;
            m.prepare (rate);

            for (const auto& c : control::buttons())
                if (c.command == control::Command::pageSound)
                {
                    m.command (c, true);
                    m.command (c, false);
                }

            const int index = m.selectedDrumIndex();
            expect (m.engine().drum (index).playMode == PlayMode::unaVolta);

            m.encoder (control::forEncoder (71 + 6), 1);
            expect (m.engine().drum (index).playMode == PlayMode::tenuto);

            m.encoder (control::forEncoder (71 + 6), 1);
            m.encoder (control::forEncoder (71 + 6), 1);
            expect (m.engine().drum (index).playMode == PlayMode::avviaFerma);

            m.encoder (control::forEncoder (71 + 6), 1);
            expect (m.engine().drum (index).playMode == PlayMode::avviaFerma,
                    "e' andato oltre l'ultimo modo");

            // e il display lo dice
            expect (m.screenLines().joinIntoString (" ").contains ("AVVIA FERMA"),
                    "il modo non si vede sul display");
        }

        beginTest ("il modo resta nella scena");
        {
            auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("rebirth-modi-" + juce::String (juce::Random::getSystemRandom().nextInt (100000)));
            temp.createDirectory();

            machine::Machine m;
            m.prepare (rate);
            m.engine().drum (m.selectedDrumIndex()).playMode = PlayMode::giro;

            juce::String error;
            const auto folder = temp.getChildFile ("modi");
            expect (library::saveScene (folder, m, error), error);

            machine::Machine other;
            other.prepare (rate);
            expect (library::loadScene (folder, other, error), error);
            expect (other.engine().drum (other.selectedDrumIndex()).playMode == PlayMode::giro,
                    "il modo non e' tornato");

            temp.deleteRecursively();
        }

        beginTest ("i nomi dei modi vanno e tornano");
        {
            for (auto mode : { PlayMode::unaVolta, PlayMode::tenuto,
                               PlayMode::giro, PlayMode::avviaFerma })
                expect (playModeFromName (playModeName (mode)) == mode,
                        "non torna: " + playModeName (mode));
        }
    }
};

static PlayModeTests playModeTests;
