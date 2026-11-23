#pragma once

#include "engine/SoundKind.h"

#include <juce_audio_basics/juce_audio_basics.h>

// Un suono dei 128 della macchina, con i parametri che aveva l'originale.
struct Drum
{
    juce::AudioBuffer<float> sample;
    double sourceRate = 44100.0;
    juce::String name;
    sound::Kind kind = sound::Kind::generico;   // decide il colore del pad

    float level = 0.8f;          // 0..1
    int tuneSemitones = 0;       // -24..+24, come l'accordatura dell'MPC
    float pan = 0.0f;            // -1 sinistra, +1 destra
    int start = 0;               // primo campione
    int end = 0;                 // ultimo campione, 0 = fino in fondo
    bool reverse = false;
    int muteGroup = 0;           // 0 = nessuno; i suoni dello stesso gruppo si tagliano

    bool loaded() const { return sample.getNumSamples() > 0; }

    int lastSample() const
    {
        const int n = sample.getNumSamples();
        return end > 0 ? juce::jlimit (0, n, end) : n;
    }
};
