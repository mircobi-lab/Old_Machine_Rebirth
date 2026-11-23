#include "Recorder.h"

void Recorder::prepare (double sampleRate, int numChannels)
{
    rate = sampleRate > 0 ? sampleRate : 44100.0;
    channels = juce::jlimit (1, 2, numChannels);

    // la memoria si prende adesso, una volta per tutte: nel filo dell'audio
    // non si alloca niente
    take.setSize (channels, (int) (rate * maxLengthSeconds) + 8, false, true, true);
    pre.setSize (channels, (int) (rate * maxPreRollSeconds) + 8, false, true, true);
    take.clear();
    pre.clear();

    preWrite = 0;
    preFilled = 0;
    written.store (0);
    current.store (State::fermo);
    level.store (0.0f);
}

void Recorder::setThreshold (int percent)
{
    thresholdPercent = juce::jlimit (0, 99, percent);
}

void Recorder::setLengthSeconds (double seconds)
{
    takeLength = juce::jlimit (1.0, maxLengthSeconds, seconds);
}

void Recorder::setPreRollSeconds (double seconds)
{
    preRoll = juce::jlimit (0.0, maxPreRollSeconds, seconds);
}

float Recorder::thresholdLinear() const
{
    if (thresholdPercent <= 0)
        return 0.0f;

    // la scala e' quadratica: in basso si sente anche un respiro, in alto
    // serve un colpo
    const auto x = (float) thresholdPercent / 99.0f;
    return 0.5f * x * x;
}

double Recorder::recordedSeconds() const
{
    return (double) written.load() / rate;
}

void Recorder::arm()
{
    written.store (0);
    current.store (State::armato);
}

void Recorder::startNow()
{
    written.store (0);
    beginWriting();
    current.store (State::registra);
}

void Recorder::stop()
{
    if (current.load() == State::registra && written.load() > 0)
        current.store (State::pieno);
    else
        current.store (State::fermo);
}

void Recorder::cancel()
{
    written.store (0);
    current.store (State::fermo);
}

void Recorder::beginWriting()
{
    // l'anticipo entra per primo: e' il suono di prima dello scatto, tirato
    // fuori dall'anello nell'ordine giusto
    const int wanted = juce::jmin (preFilled, (int) (preRoll * rate));
    if (wanted <= 0)
        return;

    const int ringSize = pre.getNumSamples();
    int from = preWrite - wanted;
    while (from < 0)
        from += ringSize;

    for (int ch = 0; ch < channels; ++ch)
    {
        int remaining = wanted;
        int source = from;
        int destination = 0;

        while (remaining > 0)
        {
            const int block = juce::jmin (remaining, ringSize - source);
            take.copyFrom (ch, destination, pre, ch, source, block);
            destination += block;
            remaining -= block;
            source = (source + block) % ringSize;
        }
    }

    written.store (wanted);
}

void Recorder::processInput (const float* const* input, int numChannels, int numSamples)
{
    if (numSamples <= 0 || numChannels <= 0 || take.getNumSamples() == 0)
        return;

    const int used = juce::jmin (channels, numChannels);

    // il livello che si vede nella barra, con discesa lenta per poterlo leggere
    float peak = 0.0f;
    for (int ch = 0; ch < used; ++ch)
        peak = juce::jmax (peak, juce::FloatVectorOperations::findMaximum (input[ch], numSamples),
                           -juce::FloatVectorOperations::findMinimum (input[ch], numSamples));

    const auto shown = level.load();
    level.store (peak > shown ? peak : shown * 0.8f + peak * 0.2f);

    const auto state = current.load();

    if (state == State::armato || state == State::fermo)
    {
        // anche fermo l'anello continua a girare: quando si arma, l'anticipo
        // e' gia' pieno
        const int ringSize = pre.getNumSamples();
        for (int i = 0; i < numSamples; ++i)
        {
            const int at = (preWrite + i) % ringSize;
            for (int ch = 0; ch < channels; ++ch)
                pre.setSample (ch, at, input[juce::jmin (ch, used - 1)][i]);
        }

        preWrite = (preWrite + numSamples) % ringSize;
        preFilled = juce::jmin (ringSize, preFilled + numSamples);

        if (state == State::armato && peak >= thresholdLinear())
        {
            beginWriting();
            current.store (State::registra);
        }
        else
        {
            return;
        }
    }

    if (current.load() != State::registra)
        return;

    const int limit = juce::jmin (take.getNumSamples(), (int) (takeLength * rate));
    int at = written.load();
    const int room = juce::jmax (0, limit - at);
    const int toWrite = juce::jmin (room, numSamples);

    for (int ch = 0; ch < channels; ++ch)
        take.copyFrom (ch, at, input[juce::jmin (ch, used - 1)], toWrite);

    at += toWrite;
    written.store (at);

    if (at >= limit)
        current.store (State::pieno);
}

bool Recorder::takeRecording (juce::AudioBuffer<float>& destination)
{
    if (current.load() != State::pieno)
        return false;

    const int length = written.load();
    if (length <= 0)
    {
        current.store (State::fermo);
        return false;
    }

    destination.setSize (channels, length, false, true, true);
    for (int ch = 0; ch < channels; ++ch)
        destination.copyFrom (ch, 0, take, ch, 0, length);

    written.store (0);
    current.store (State::fermo);
    return true;
}
