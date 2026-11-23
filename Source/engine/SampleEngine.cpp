#include "SampleEngine.h"

SampleEngine::SampleEngine()
{
    formats.registerBasicFormats();
}

void SampleEngine::prepare (double sampleRate)
{
    rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    stopAll();
}

Drum& SampleEngine::drum (int index)
{
    return drums[(size_t) juce::jlimit (0, machine::numDrums - 1, index)];
}

const Drum& SampleEngine::drum (int index) const
{
    return drums[(size_t) juce::jlimit (0, machine::numDrums - 1, index)];
}

int SampleEngine::loadedCount() const
{
    int n = 0;
    for (const auto& d : drums)
        if (d.loaded())
            ++n;
    return n;
}

void SampleEngine::trigger (int drumIndex, float velocity)
{
    if (! juce::isPositiveAndBelow (drumIndex, machine::numDrums))
        return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (1, start1, size1, start2, size2);

    const Hit hit { drumIndex, juce::jlimit (0.0f, 1.0f, velocity) };
    if (size1 > 0)
        queue[(size_t) start1] = hit;
    else if (size2 > 0)
        queue[(size_t) start2] = hit;

    fifo.finishedWrite (size1 + size2);
}

void SampleEngine::stopAll()
{
    for (auto& v : voices)
        v.active = false;
    playing.store (0);
}

double SampleEngine::incrementFor (const Drum& d) const
{
    const double pitch = std::pow (2.0, (double) d.tuneSemitones / 12.0);
    return pitch * d.sourceRate / rate;
}

void SampleEngine::startVoice (int drumIndex, float velocity)
{
    if (! juce::isPositiveAndBelow (drumIndex, machine::numDrums))
        return;

    const auto& d = drums[(size_t) drumIndex];
    if (! d.loaded())
        return;

    // Gruppo di taglio: il charleston chiuso spegne quello aperto
    if (d.muteGroup > 0)
        for (auto& v : voices)
            if (v.active && v.drum >= 0 && drums[(size_t) v.drum].muteGroup == d.muteGroup)
                v.active = false;

    Voice* chosen = nullptr;
    for (auto& v : voices)
        if (! v.active)
        {
            chosen = &v;
            break;
        }

    if (chosen == nullptr)
    {
        // tutte occupate: si ruba la piu' vecchia
        chosen = &voices[0];
        for (auto& v : voices)
            if (v.startedAt < chosen->startedAt)
                chosen = &v;
    }

    chosen->drum = drumIndex;
    chosen->increment = incrementFor (d);
    chosen->position = d.reverse ? (double) (d.lastSample() - 1) : (double) d.start;
    chosen->gain = velocity * velocity * d.level;   // la velocity scalda in modo naturale
    chosen->startedAt = ++counter;
    chosen->active = true;
}

void SampleEngine::process (juce::AudioBuffer<float>& buffer)
{
    buffer.clear();

    // i colpi arrivati da fuori
    int start1, size1, start2, size2;
    fifo.prepareToRead (queueSize, start1, size1, start2, size2);
    for (int i = 0; i < size1; ++i)
        startVoice (queue[(size_t) (start1 + i)].drum, queue[(size_t) (start1 + i)].velocity);
    for (int i = 0; i < size2; ++i)
        startVoice (queue[(size_t) (start2 + i)].drum, queue[(size_t) (start2 + i)].velocity);
    fifo.finishedRead (size1 + size2);

    const juce::ScopedTryLock sl (loadLock);
    if (! sl.isLocked())
        return;           // si sta caricando un suono: questo blocco resta muto

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0)
        return;

    int active = 0;

    // la voce piu' giovane fra quelle che suonano: e' quella che si segue col
    // segno sull'onda
    int newest = -1;
    std::uint32_t newestAge = 0;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        const auto& d = drums[(size_t) v.drum];
        const auto& source = d.sample;
        const int last = d.lastSample();
        const int sourceChannels = source.getNumChannels();

        if (sourceChannels == 0 || last < 2)
        {
            v.active = false;
            continue;
        }

        const float leftGain  = v.gain * std::sqrt (0.5f * (1.0f - d.pan));
        const float rightGain = v.gain * std::sqrt (0.5f * (1.0f + d.pan));

        for (int i = 0; i < numSamples; ++i)
        {
            const int index = (int) v.position;
            if (index < 0 || index >= last - 1)
            {
                v.active = false;
                break;
            }

            const float frac = (float) (v.position - (double) index);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto* in = source.getReadPointer (juce::jmin (ch, sourceChannels - 1));
                const float s = in[index] + frac * (in[index + 1] - in[index]);
                buffer.addSample (ch, i, s * (ch == 0 ? leftGain : rightGain));
            }

            v.position += d.reverse ? -v.increment : v.increment;
        }

        if (v.active)
        {
            ++active;

            if (newest < 0 || v.startedAt > newestAge)
            {
                newest = v.drum;
                newestAge = v.startedAt;

                const int length = juce::jmax (1, d.sample.getNumSamples());
                shownPosition.store ((float) juce::jlimit (0.0, 1.0, v.position / (double) length));
            }
        }
    }

    shownDrum.store (newest);
    playing.store (active);
}

double SampleEngine::positionOf (int drumIndex) const
{
    if (shownDrum.load() != drumIndex)
        return -1.0;

    return (double) shownPosition.load();
}

bool SampleEngine::loadBuffer (int drumIndex, const juce::String& name,
                               const juce::AudioBuffer<float>& buffer, double sourceRate,
                               sound::Kind kind)
{
    if (! juce::isPositiveAndBelow (drumIndex, machine::numDrums) || buffer.getNumSamples() <= 0)
        return false;

    juce::AudioBuffer<float> copy (buffer);

    const juce::ScopedLock sl (loadLock);
    auto& d = drums[(size_t) drumIndex];
    d.sample = std::move (copy);
    d.sourceRate = sourceRate > 0 ? sourceRate : rate;
    d.name = name.toUpperCase();
    // se chi carica sa di che famiglia e' il suono lo dice, altrimenti si
    // indovina dal nome
    d.kind = kind != sound::Kind::generico ? kind : sound::guessFromName (d.name);
    d.start = 0;
    d.end = 0;
    d.reverse = false;
    loads.fetch_add (1);
    return true;
}

juce::AudioBuffer<float> SampleEngine::copyOf (int drumIndex) const
{
    if (! juce::isPositiveAndBelow (drumIndex, machine::numDrums))
        return {};

    const juce::ScopedLock sl (loadLock);
    return drums[(size_t) drumIndex].sample;
}

bool SampleEngine::loadFile (int drumIndex, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (drumIndex, machine::numDrums) || ! file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    // un minuto al massimo per suono: oltre non e' piu' un colpo di batteria
    const int length = (int) juce::jmin (reader->lengthInSamples,
                                         (juce::int64) (reader->sampleRate * 60.0));
    juce::AudioBuffer<float> buffer ((int) juce::jmin (reader->numChannels, 2u), length);
    reader->read (&buffer, 0, length, 0, true, true);

    const juce::ScopedLock sl (loadLock);
    auto& d = drums[(size_t) drumIndex];
    d.sample = std::move (buffer);
    d.sourceRate = reader->sampleRate > 0 ? reader->sampleRate : rate;
    d.name = file.getFileNameWithoutExtension().toUpperCase();
    d.kind = sound::guessFromName (d.name);
    d.start = 0;
    d.end = 0;
    d.reverse = false;
    loads.fetch_add (1);
    return true;
}
