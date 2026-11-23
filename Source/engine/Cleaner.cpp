#include "Cleaner.h"

#include "Dsp/SpectralCleaner.h"

namespace cleaner
{

juce::String presetName (Preset p)
{
    return p == Preset::ai ? "AI" : "MP3";
}

static asf::CleanSettings settingsFor (Preset p)
{
    asf::CleanSettings s;

    if (p == Preset::ai)
    {
        // la patina delle canzoni generate: fruscio in alto, medi impastati,
        // sibilanti dure
        s.mud = 0.45f;
        s.harsh = 0.40f;
        s.sens = 0.55f;
        s.fizz = 0.55f;
        s.fizzFreq = 11000.0f;
        s.deess = 0.30f;
        s.vocal = 0.15f;
        s.air = 0.20f;
        s.bassMono = 0.0f;
    }
    else
    {
        // mp3 a basso bitrate: il guaio sta quasi tutto sopra i 10 kHz
        s.mud = 0.20f;
        s.harsh = 0.25f;
        s.sens = 0.5f;
        s.fizz = 0.65f;
        s.fizzFreq = 10000.0f;
        s.deess = 0.15f;
        s.vocal = 0.0f;
        s.air = 0.25f;
        s.bassMono = 0.0f;
    }

    return s;
}

bool clean (juce::AudioBuffer<float>& buffer, double sampleRate, Preset preset)
{
    const int length = buffer.getNumSamples();
    if (length <= 0 || buffer.getNumChannels() <= 0)
        return false;

    asf::SpectralCleaner engine;
    engine.prepare (sampleRate > 0.0 ? sampleRate : 44100.0);
    engine.reset();

    const auto settings = settingsFor (preset);
    const int latency = engine.getLatency();

    // Il pulitore lavora a frame e restituisce il suono in ritardo di una
    // finestra: si aggiunge la coda in fondo, si elabora tutto e poi si
    // riallinea togliendo quel ritardo, cosi' il campione resta a tempo.
    const int channels = juce::jmin (2, buffer.getNumChannels());
    juce::AudioBuffer<float> work (2, length + latency * 2);
    work.clear();

    for (int ch = 0; ch < 2; ++ch)
        work.copyFrom (ch, 0, buffer, juce::jmin (ch, channels - 1), 0, length);

    engine.process (work.getWritePointer (0), work.getWritePointer (1),
                    work.getNumSamples(), settings);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, work, juce::jmin (ch, 1), latency, length);

    return true;
}

} // namespace cleaner
