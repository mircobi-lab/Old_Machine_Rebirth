#include "Warp.h"

namespace warp
{

namespace
{
constexpr int frameSize = 2048;          // il pezzetto che si incolla
constexpr int synthesisHop = frameSize / 4;
constexpr int searchRadius = frameSize / 8;

// Quanto si somigliano due pezzi: la somma dei prodotti, che e' alta quando
// le onde vanno su e giu' insieme.
float similarity (const float* a, const float* b, int length)
{
    float sum = 0.0f;
    for (int i = 0; i < length; ++i)
        sum += a[i] * b[i];

    return sum;
}
}

juce::AudioBuffer<float> stretch (const juce::AudioBuffer<float>& source, double factor)
{
    factor = juce::jlimit (0.25, 4.0, factor);

    const int length = source.getNumSamples();
    const int channels = source.getNumChannels();

    if (length < frameSize * 2 || channels <= 0)
        return source;

    const int outputLength = (int) std::llround (length * factor);
    const int analysisHop = juce::jmax (1, (int) std::llround (synthesisHop / factor));

    juce::AudioBuffer<float> out (channels, outputLength + frameSize);
    out.clear();

    // la finestra: sale e scende, cosi' i pezzi incollati non fanno gradini
    std::vector<float> window ((size_t) frameSize);
    for (int i = 0; i < frameSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi
                                                                * i / (frameSize - 1));

    std::vector<float> tail ((size_t) frameSize, 0.0f);   // la coda dell'ultimo pezzo
    int readAt = 0;
    int writeAt = 0;
    bool first = true;

    while (writeAt + frameSize < out.getNumSamples() && readAt + frameSize < length)
    {
        int best = readAt;

        // si cerca intorno al punto giusto il pezzo che attacca meglio con
        // quello appena scritto: e' questo che tiene il tono e il ritmo
        if (! first)
        {
            float bestScore = -std::numeric_limits<float>::max();
            const auto* reference = tail.data();

            for (int offset = -searchRadius; offset <= searchRadius; ++offset)
            {
                const int candidate = readAt + offset;
                if (candidate < 0 || candidate + synthesisHop >= length)
                    continue;

                const float score = similarity (source.getReadPointer (0) + candidate,
                                                reference, synthesisHop);
                if (score > bestScore)
                {
                    bestScore = score;
                    best = candidate;
                }
            }
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* in = source.getReadPointer (ch);
            auto* dest = out.getWritePointer (ch);

            for (int i = 0; i < frameSize; ++i)
            {
                const int from = best + i;
                if (from >= length)
                    break;

                dest[writeAt + i] += in[from] * window[(size_t) i];
            }
        }

        // la coda che servira' per cercare il prossimo attacco
        for (int i = 0; i < frameSize; ++i)
        {
            const int from = best + synthesisHop + i;
            tail[(size_t) i] = from < length ? source.getSample (0, from) : 0.0f;
        }

        readAt = best + analysisHop;
        writeAt += synthesisHop;
        first = false;
    }

    out.setSize (channels, outputLength, true, true, true);
    return out;
}

double factorForTempo (int lengthInSamples, double sampleRate, double bpm, int& bars)
{
    bars = 1;

    if (lengthInSamples <= 0 || sampleRate <= 0 || bpm <= 0)
        return 1.0;

    const double seconds = (double) lengthInSamples / sampleRate;
    const double secondsPerBar = 4.0 * 60.0 / bpm;

    // la durata in battute piu' vicina, fra quelle che hanno senso
    const double exact = seconds / secondsPerBar;
    double bestDistance = std::numeric_limits<double>::max();
    double target = secondsPerBar;

    for (int candidate : { 1, 2, 4, 8, 16 })
    {
        const double distance = std::abs (exact - candidate);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bars = candidate;
            target = candidate * secondsPerBar;
        }
    }

    // mezza battuta, per i pezzetti corti
    if (exact < 0.75)
    {
        bars = 1;
        target = secondsPerBar * (exact < 0.375 ? 0.25 : 0.5);
    }

    return juce::jlimit (0.25, 4.0, target / seconds);
}

} // namespace warp
