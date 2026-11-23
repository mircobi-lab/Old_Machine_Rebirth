#include "engine/SampleEngine.h"

#include <juce_core/juce_core.h>

class SampleEngineTests final : public juce::UnitTest
{
public:
    SampleEngineTests() : juce::UnitTest ("Campionatore", "Macchina") {}

    // Un suono finto: una rampa, cosi' si riconosce dove sta leggendo
    static juce::AudioBuffer<float> ramp (int length)
    {
        juce::AudioBuffer<float> b (1, length);
        for (int i = 0; i < length; ++i)
            b.setSample (0, i, (float) i / (float) length);
        return b;
    }

    static float peak (const juce::AudioBuffer<float>& b)
    {
        return b.getMagnitude (0, b.getNumSamples());
    }

    void runTest() override
    {
        beginTest ("un pad vuoto non fa rumore");
        {
            SampleEngine e;
            e.prepare (44100.0);
            e.trigger (0, 1.0f);

            juce::AudioBuffer<float> out (2, 256);
            e.process (out);
            expectEquals (peak (out), 0.0f);
            expectEquals (e.activeVoices(), 0);
        }

        beginTest ("un pad con un suono suona");
        {
            SampleEngine e;
            e.prepare (44100.0);
            e.drum (3).sample = ramp (4096);
            e.drum (3).sourceRate = 44100.0;
            e.drum (3).level = 1.0f;

            juce::AudioBuffer<float> out (2, 256);
            e.trigger (3, 1.0f);
            e.process (out);

            expect (peak (out) > 0.0f);
            expectEquals (e.activeVoices(), 1);
        }

        beginTest ("la velocity decide quanto forte");
        {
            SampleEngine e;
            e.prepare (44100.0);
            e.drum (0).sample = ramp (4096);
            e.drum (0).level = 1.0f;

            juce::AudioBuffer<float> forte (2, 512), piano (2, 512);
            e.trigger (0, 1.0f);
            e.process (forte);
            e.stopAll();
            e.trigger (0, 0.3f);
            e.process (piano);

            expect (peak (forte) > peak (piano) * 2.0f);
        }

        beginTest ("le voci sono 32 e la piu' vecchia lascia il posto");
        {
            SampleEngine e;
            e.prepare (44100.0);
            for (int i = 0; i < 40; ++i)
            {
                e.drum (i).sample = ramp (44100);
                e.drum (i).level = 1.0f;
            }

            juce::AudioBuffer<float> out (2, 64);
            for (int i = 0; i < 40; ++i)
            {
                e.trigger (i, 1.0f);
                e.process (out);
            }

            expectEquals (e.activeVoices(), SampleEngine::numVoices);
        }

        beginTest ("il gruppo di taglio spegne il suono di prima");
        {
            SampleEngine e;
            e.prepare (44100.0);
            for (int i : { 2, 3 })
            {
                e.drum (i).sample = ramp (44100);
                e.drum (i).muteGroup = 1;
                e.drum (i).level = 1.0f;
            }

            juce::AudioBuffer<float> out (2, 64);
            e.trigger (2, 1.0f);
            e.process (out);
            expectEquals (e.activeVoices(), 1);

            e.trigger (3, 1.0f);
            e.process (out);
            expectEquals (e.activeVoices(), 1, "il charleston aperto deve zittirsi");
        }

        beginTest ("l'accordatura cambia la velocita' di lettura");
        {
            // Con la rampa dentro, il valore che esce dice a che punto del
            // campione siamo arrivati: un'ottava sopra ci arriva al doppio.
            auto readAfter = [] (int semitones)
            {
                SampleEngine e;
                e.prepare (44100.0);
                e.drum (0).sample = ramp (44100);
                e.drum (0).level = 1.0f;
                e.drum (0).tuneSemitones = semitones;

                juce::AudioBuffer<float> out (1, 1000);
                e.trigger (0, 1.0f);
                e.process (out);
                return out.getSample (0, 999);
            };

            const float dritto = readAfter (0);
            const float ottavaSopra = readAfter (12);
            const float ottavaSotto = readAfter (-12);

            expect (dritto > 0.0f);
            expectWithinAbsoluteError (ottavaSopra / dritto, 2.0f, 0.02f);
            expectWithinAbsoluteError (ottavaSotto / dritto, 0.5f, 0.02f);
        }

        beginTest ("il kit di partenza riempie i primi otto pad");
        {
            SampleEngine e;
            e.prepare (44100.0);
            e.loadStarterKit();

            expectEquals (e.loadedCount(), 8);
            expectEquals (e.drum (0).name, juce::String ("CASSA"));
            expect (e.drum (0).sample.getNumSamples() > 1000);
            expect (e.drum (2).muteGroup == e.drum (3).muteGroup,
                    "i due charleston stanno nello stesso gruppo");

            juce::AudioBuffer<float> out (2, 512);
            e.trigger (0, 1.0f);
            e.process (out);
            expect (peak (out) > 0.05f, "la cassa deve sentirsi");
        }
    }
};

static SampleEngineTests sampleEngineTests;
