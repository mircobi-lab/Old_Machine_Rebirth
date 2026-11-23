#pragma once

#include "engine/Drum.h"
#include "engine/SoundKind.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <vector>

// Spacca un brano in fette e le mette sui pad.
//
// Il lavoro vero lo fa il motore di Sample Forge, che e' gia' scritto e
// collaudato nel repository mircobi-lab/sample-forge: separa il brano in
// batteria, voce, basso e resto, trova il tempo e la tonalita', e ritaglia i
// pezzi buoni. Qui dentro c'e' solo la traduzione da quel risultato ai nostri
// pad.
namespace chopper
{

struct Slice
{
    juce::String name;      // come si chiamera' sul pad
    juce::String bank;      // "BATTERIA", "VOCE", "GIRI", "ACCORDI", "BASSO"
    sound::Kind kind = sound::Kind::generico;
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
};

struct Result
{
    std::vector<Slice> slices;
    double bpm = 0.0;
    juce::String key;        // la tonalita', come la scrive Sample Forge
    juce::String error;      // vuoto se e' andata bene
    bool ok() const { return error.isEmpty() && ! slices.empty(); }
};

// Legge il file, lo analizza e restituisce le fette. Puo' metterci parecchio su
// un brano intero: va chiamata fuori dal filo dell'audio e fuori da quello dei
// messaggi. `progress` viene chiamata di tanto in tanto, da 0 a 1.
Result chop (const juce::File&, int secondsToUse = 60,
             std::function<void (double, juce::String)> progress = {});

} // namespace chopper
