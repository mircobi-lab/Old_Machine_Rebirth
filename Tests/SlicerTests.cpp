#include "engine/Slicer.h"
#include "machine/Machine.h"

#include <juce_core/juce_core.h>

// Il taglio a fette uguali, quello dei mash up: un brano lungo sparso su tanti
// pad. Si controlla che le fette tornino, che non si perda suono per strada e
// che ai capi non ci sia il clic.
class SlicerTests final : public juce::UnitTest
{
public:
    SlicerTests() : juce::UnitTest ("Fette uguali", "Macchina") {}

    static juce::AudioBuffer<float> tone (double rate, double seconds, double hz = 220.0)
    {
        const int length = (int) (rate * seconds);
        juce::AudioBuffer<float> b (2, length);
        for (int i = 0; i < length; ++i)
        {
            const auto v = 0.7f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz
                                                        * (double) i / rate);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    void runTest() override
    {
        const double rate = 44100.0;

        beginTest ("taglia nel numero di fette chiesto");
        {
            const auto source = tone (rate, 8.0);
            const auto list = slicer::cut (source, rate, 16, "BRANO");
            expectEquals ((int) list.size(), 16);

            for (const auto& s : list)
            {
                expect (s.audio.getNumSamples() > 0);
                expectEquals (s.audio.getNumChannels(), 2);
                expect (s.name.startsWith ("BRANO"));
            }
        }

        beginTest ("le fette insieme fanno il campione intero");
        {
            const auto source = tone (rate, 4.0);
            const auto list = slicer::cut (source, rate, 8, "GIRO");

            int total = 0;
            for (const auto& s : list)
                total += s.audio.getNumSamples();

            expectWithinAbsoluteError ((double) total, (double) source.getNumSamples(), 64.0);
        }

        beginTest ("ai capi di ogni fetta non c'e' il clic");
        {
            const auto source = tone (rate, 4.0);
            const auto list = slicer::cut (source, rate, 32, "GIRO");

            for (const auto& s : list)
            {
                const int n = s.audio.getNumSamples();
                expect (std::abs (s.audio.getSample (0, 0)) < 0.01f,
                        "l'attacco parte da " + juce::String (s.audio.getSample (0, 0)));
                expect (std::abs (s.audio.getSample (0, n - 1)) < 0.01f,
                        "la coda finisce a " + juce::String (s.audio.getSample (0, n - 1)));
                expect (s.audio.getMagnitude (0, n) > 0.3f, "la fetta e' muta");
            }
        }

        beginTest ("un campione troppo corto non si taglia in 128");
        {
            const auto source = tone (rate, 0.2);
            expect (slicer::cut (source, rate, 128, "CORTO").empty());
        }

        beginTest ("il giro dei numeri di fette");
        {
            expectEquals (slicer::nextCount (16, 1), 32);
            expectEquals (slicer::nextCount (16, -1), 8);
            expectEquals (slicer::nextCount (128, 1), 128);     // si ferma in cima
            expectEquals (slicer::nextCount (4, -1), 4);        // e in fondo
        }

        beginTest ("le fette vanno sui pad dal pad scelto in avanti");
        {
            machine::Machine m;
            m.prepare (rate);

            std::vector<chopper::Slice> list;
            for (int i = 0; i < 4; ++i)
            {
                chopper::Slice s;
                s.name = "FETTA" + juce::String (i + 1);
                s.bank = "FETTE";
                s.kind = sound::Kind::giro;
                s.audio.setSize (1, 1000);
                s.sampleRate = rate;
                list.push_back (std::move (s));
            }

            // pad 5 del banco B
            const machine::PadRef start { 1, 5 };
            expectEquals (m.placeFrom (start, list), 4);

            const int from = machine::drumIndex (start);
            expectEquals (m.engine().drum (from).name, juce::String ("FETTA1"));
            expectEquals (m.engine().drum (from + 3).name, juce::String ("FETTA4"));
            expect (m.selectedPad().bank == start.bank && m.selectedPad().pad == start.pad);
        }

        beginTest ("un brano lungo sul pad riempie i pad che seguono");
        {
            machine::Machine m;
            m.prepare (rate);

            const auto song = tone (rate, 10.0, 110.0);
            const int index = m.selectedDrumIndex();
            expect (m.engine().loadBuffer (index, "BRANO", song, rate));

            const int placed = m.fillFromSelected();
            expectEquals (placed, m.sliceCount());

            for (int i = 0; i < placed; ++i)
            {
                const auto& d = m.engine().drum (index + i);
                expect (d.loaded(), "pad " + juce::String (i) + " vuoto");
                expect (d.sample.getNumSamples() < song.getNumSamples() / 2,
                        "la fetta e' lunga come il brano");
            }
        }

        beginTest ("oltre il pad 128 si ferma invece di scrivere fuori");
        {
            machine::Machine m;
            m.prepare (rate);

            std::vector<chopper::Slice> list;
            for (int i = 0; i < 8; ++i)
            {
                chopper::Slice s;
                s.name = "F" + juce::String (i);
                s.bank = "FETTE";
                s.audio.setSize (1, 500);
                s.sampleRate = rate;
                list.push_back (std::move (s));
            }

            // ultimo banco, ultimi pad
            const machine::PadRef start { machine::numBanks - 1, machine::padsPerBank - 2 };
            const int placed = m.placeFrom (start, list);
            expect (placed > 0 && placed <= 3, "ne ha messe " + juce::String (placed));
        }
    }
};

static SlicerTests slicerTests;
