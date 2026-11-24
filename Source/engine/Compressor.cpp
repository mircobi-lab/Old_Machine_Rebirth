#include "Compressor.h"

void Compressor::prepare (double sampleRate)
{
    rate = sampleRate > 0 ? sampleRate : 44100.0;
    envelope = 0.0;
    reduction = 0.0f;
    updateCoefficients();
}

void Compressor::updateCoefficients()
{
    // le costanti di tempo: quanto ci mette a scendere e a risalire
    attackCoeff = std::exp (-1.0 / (juce::jmax (0.1, attack) * 0.001 * rate));
    releaseCoeff = std::exp (-1.0 / (juce::jmax (1.0, release) * 0.001 * rate));
}

void Compressor::setThresholdDb (double db)
{
    threshold = juce::jlimit (-40.0, 0.0, db);
}

void Compressor::setRatio (double r)
{
    amount = juce::jlimit (1.0, 20.0, r);
}

void Compressor::setAttackMs (double ms)
{
    attack = juce::jlimit (1.0, 100.0, ms);
    updateCoefficients();
}

void Compressor::setReleaseMs (double ms)
{
    release = juce::jlimit (20.0, 1000.0, ms);
    updateCoefficients();
}

void Compressor::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    // quello che si recupera in volume dopo aver tirato giu': senza, ogni
    // volta che si schiaccia si abbassa anche tutto il resto
    const double makeup = std::pow (10.0, (-threshold * (1.0 - 1.0 / amount)) / 40.0);

    float worst = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // il picco dei due canali insieme
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

        const double db = juce::Decibels::gainToDecibels ((double) peak, -100.0);
        const double over = juce::jmax (0.0, db - threshold);

        const double coeff = over > envelope ? attackCoeff : releaseCoeff;
        envelope = over + coeff * (envelope - over);

        const double cut = envelope * (1.0 - 1.0 / amount);
        const double gain = juce::Decibels::decibelsToGain (-cut) * makeup;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, i, (float) (buffer.getSample (ch, i) * gain));

        worst = juce::jmax (worst, (float) cut);
    }

    reduction = worst;
}
