#include "engine/Recorder.h"
#include "machine/Machine.h"

#include <juce_core/juce_core.h>

// Il registratore. Il campionamento vero si prova col microfono, ma la soglia,
// l'anticipo e la durata si possono provare qui dandogli del suono finto.
class RecorderTests final : public juce::UnitTest
{
public:
    RecorderTests() : juce::UnitTest ("Registratore", "Macchina") {}

    // Manda al registratore un blocco di suono a volume fisso.
    static void feed (Recorder& r, float amplitude, int numSamples, double phase = 0.0)
    {
        std::vector<float> left ((size_t) numSamples), right ((size_t) numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto v = amplitude * (float) std::sin (phase + 0.05 * i);
            left[(size_t) i] = v;
            right[(size_t) i] = v;
        }

        const float* channels[2] = { left.data(), right.data() };
        r.processInput (channels, 2, numSamples);
    }

    void runTest() override
    {
        const double rate = 44100.0;

        beginTest ("appena preparato e' fermo e non registra niente");
        {
            Recorder r;
            r.prepare (rate, 2);
            expect (r.state() == Recorder::State::fermo);
            feed (r, 0.9f, 512);
            expect (! r.isRecording());
            expect (! r.hasTake());
        }

        beginTest ("armato, parte solo quando il suono supera la soglia");
        {
            Recorder r;
            r.prepare (rate, 2);
            r.setThreshold (50);
            r.setPreRollSeconds (0.0);
            r.setLengthSeconds (1.0);
            r.arm();

            feed (r, 0.02f, 1024);                 // troppo piano
            expect (r.isWaiting(), "e' partito con un suono sotto la soglia");

            feed (r, 0.95f, 1024);                 // adesso basta
            expect (r.isRecording() || r.hasTake(), "non e' partito col colpo");
        }

        beginTest ("con soglia a zero parte subito");
        {
            Recorder r;
            r.prepare (rate, 2);
            r.setThreshold (0);
            r.setLengthSeconds (1.0);
            r.arm();
            feed (r, 0.001f, 256);
            expect (r.isRecording() || r.hasTake());
        }

        beginTest ("l'anticipo recupera il suono di prima dello scatto");
        {
            Recorder r;
            r.prepare (rate, 2);
            r.setThreshold (60);
            r.setPreRollSeconds (0.3);
            r.setLengthSeconds (2.0);

            // mezzo secondo di suono piano prima di armare: finisce nell'anello
            feed (r, 0.05f, (int) (rate * 0.5));
            r.arm();
            feed (r, 0.05f, 2048);
            expect (r.isWaiting());

            feed (r, 0.9f, 2048);
            r.stop();

            juce::AudioBuffer<float> take;
            expect (r.takeRecording (take), "non ha tenuto niente");

            // dentro ci deve stare l'anticipo: piu' dei soli 2048 del colpo
            expect (take.getNumSamples() > 2048 + (int) (rate * 0.2),
                    "l'anticipo non c'e': " + juce::String (take.getNumSamples()) + " campioni");

            // e il suono piano dell'anticipo si deve sentire all'inizio
            expect (std::abs (take.getSample (0, 10)) > 0.001f, "l'inizio e' muto");
        }

        beginTest ("si ferma da solo alla durata scelta");
        {
            Recorder r;
            r.prepare (rate, 2);
            r.setThreshold (0);
            r.setPreRollSeconds (0.0);
            r.setLengthSeconds (1.0);
            r.startNow();

            for (int i = 0; i < 100; ++i)
                feed (r, 0.5f, 1024);              // molto piu' di un secondo

            expect (r.hasTake(), "non si e' fermato da solo");

            juce::AudioBuffer<float> take;
            expect (r.takeRecording (take));
            expectWithinAbsoluteError ((double) take.getNumSamples(), rate, rate * 0.02);
        }

        beginTest ("il campione registrato va sul pad scelto");
        {
            machine::Machine m;
            m.prepare (rate);

            auto& r = m.recorder();
            r.setThreshold (0);
            r.setPreRollSeconds (0.0);
            r.setLengthSeconds (1.0);
            r.startNow();
            for (int i = 0; i < 60; ++i)
                feed (r, 0.6f, 1024);

            expect (m.checkRecorder(), "non ha messo niente sul pad");

            const auto& d = m.engine().drum (m.selectedDrumIndex());
            expect (d.loaded());
            expect (d.name.startsWith ("REG"), "nome sbagliato: " + d.name);
            expect (d.sample.getMagnitude (0, d.sample.getNumSamples()) > 0.1f);
        }

        beginTest ("il livello dell'ingresso si vede");
        {
            Recorder r;
            r.prepare (rate, 2);
            feed (r, 0.8f, 512);
            expect (r.inputLevel() > 0.5f, "livello: " + juce::String (r.inputLevel()));
        }
    }
};

static RecorderTests recorderTests;
