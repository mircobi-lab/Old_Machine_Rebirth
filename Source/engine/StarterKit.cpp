#include "SampleEngine.h"

#include <juce_dsp/juce_dsp.h>

// Otto suoni fatti in casa, generati qui dentro invece che letti da file, cosi'
// appena si apre la macchina c'e' qualcosa da suonare senza cercare campioni.
// Non vogliono imitare nessuna macchina famosa: sono un punto di partenza.
namespace
{
constexpr double kitRate = 44100.0;

juce::AudioBuffer<float> makeBuffer (double seconds)
{
    juce::AudioBuffer<float> b (1, (int) (seconds * kitRate));
    b.clear();
    return b;
}

float envelope (int i, int length, float curve)
{
    const float t = (float) i / (float) juce::jmax (1, length);
    return std::pow (juce::jmax (0.0f, 1.0f - t), curve);
}

// rumore con un filtro passa alto di un polo, per i piatti
void addNoise (juce::AudioBuffer<float>& b, int from, int length, float amount,
               float curve, float highpass, juce::Random& random)
{
    auto* out = b.getWritePointer (0);
    float previous = 0.0f, filtered = 0.0f;

    for (int i = 0; i < length && from + i < b.getNumSamples(); ++i)
    {
        const float noise = random.nextFloat() * 2.0f - 1.0f;
        filtered = highpass * (filtered + noise - previous);
        previous = noise;
        out[from + i] += filtered * amount * envelope (i, length, curve);
    }
}

void addTone (juce::AudioBuffer<float>& b, int from, int length, double startHz, double endHz,
              float amount, float curve)
{
    auto* out = b.getWritePointer (0);
    double phase = 0.0;

    for (int i = 0; i < length && from + i < b.getNumSamples(); ++i)
    {
        const double t = (double) i / (double) juce::jmax (1, length);
        const double hz = startHz * std::pow (endHz / startHz, t);
        phase += juce::MathConstants<double>::twoPi * hz / kitRate;
        out[from + i] += (float) std::sin (phase) * amount * envelope (i, length, curve);
    }
}

juce::AudioBuffer<float> makeKick()
{
    auto b = makeBuffer (0.45);
    addTone (b, 0, (int) (0.40 * kitRate), 125.0, 45.0, 0.95f, 2.4f);
    addTone (b, 0, (int) (0.012 * kitRate), 900.0, 300.0, 0.25f, 1.0f);   // il battito della pelle
    return b;
}

juce::AudioBuffer<float> makeSnare (juce::Random& random)
{
    auto b = makeBuffer (0.32);
    addTone (b, 0, (int) (0.12 * kitRate), 210.0, 150.0, 0.45f, 2.0f);
    addTone (b, 0, (int) (0.10 * kitRate), 330.0, 240.0, 0.25f, 2.0f);
    addNoise (b, 0, (int) (0.28 * kitRate), 0.55f, 2.2f, 0.72f, random);
    return b;
}

juce::AudioBuffer<float> makeHat (double seconds, float curve, juce::Random& random)
{
    auto b = makeBuffer (seconds);
    addNoise (b, 0, (int) (seconds * kitRate * 0.95), 0.5f, curve, 0.92f, random);
    return b;
}

juce::AudioBuffer<float> makeClap (juce::Random& random)
{
    auto b = makeBuffer (0.35);
    // tre schiocchi ravvicinati e una coda, come fanno le drum machine
    for (int n = 0; n < 3; ++n)
        addNoise (b, (int) (n * 0.011 * kitRate), (int) (0.035 * kitRate), 0.45f, 3.0f, 0.8f, random);
    addNoise (b, (int) (0.033 * kitRate), (int) (0.28 * kitRate), 0.3f, 2.6f, 0.78f, random);
    return b;
}

juce::AudioBuffer<float> makeTom()
{
    auto b = makeBuffer (0.5);
    addTone (b, 0, (int) (0.45 * kitRate), 180.0, 90.0, 0.8f, 2.0f);
    return b;
}

juce::AudioBuffer<float> makeRim (juce::Random& random)
{
    auto b = makeBuffer (0.12);
    addTone (b, 0, (int) (0.05 * kitRate), 1700.0, 900.0, 0.5f, 3.0f);
    addNoise (b, 0, (int) (0.03 * kitRate), 0.3f, 3.5f, 0.85f, random);
    return b;
}

juce::AudioBuffer<float> makeCowbell()
{
    auto b = makeBuffer (0.4);
    addTone (b, 0, (int) (0.35 * kitRate), 540.0, 520.0, 0.35f, 1.6f);
    addTone (b, 0, (int) (0.35 * kitRate), 800.0, 780.0, 0.3f, 1.6f);
    return b;
}
}

void SampleEngine::loadStarterKit()
{
    juce::Random random (20261123);

    struct Entry { const char* name; juce::AudioBuffer<float> buffer; int muteGroup; float level; };

    std::vector<Entry> kit;
    kit.push_back ({ "CASSA",      makeKick(),                      0, 0.9f });
    kit.push_back ({ "RULLANTE",   makeSnare (random),              0, 0.8f });
    kit.push_back ({ "CHARLESTON", makeHat (0.09, 3.2f, random),    1, 0.6f });
    kit.push_back ({ "CHARLES AP", makeHat (0.45, 1.6f, random),    1, 0.5f });
    kit.push_back ({ "BATTIMANI",  makeClap (random),               0, 0.7f });
    kit.push_back ({ "TOM",        makeTom(),                       0, 0.8f });
    kit.push_back ({ "RIM",        makeRim (random),                0, 0.7f });
    kit.push_back ({ "CAMPANA",    makeCowbell(),                   0, 0.6f });

    const juce::ScopedLock sl (loadLock);

    for (int i = 0; i < (int) kit.size(); ++i)
    {
        auto& d = drums[(size_t) i];
        d.sample = std::move (kit[(size_t) i].buffer);
        d.sourceRate = kitRate;
        d.name = kit[(size_t) i].name;
        d.muteGroup = kit[(size_t) i].muteGroup;
        d.level = kit[(size_t) i].level;
        d.start = 0;
        d.end = 0;
        d.tuneSemitones = 0;
        d.pan = 0.0f;
        d.reverse = false;
    }
}
