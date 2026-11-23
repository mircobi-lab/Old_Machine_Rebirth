#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Ripulisce un campione rovinato: il velo metallico dell'mp3 a basso bitrate e
// la patina delle canzoni fatte con l'AI.
//
// Il lavoro lo fa il pulitore spettrale di AI Song Fixer, che sta gia' scritto
// nel repository mircobi-lab/AIsongFixer. Qui ci sono solo due dosi pronte e il
// giro per passarci dentro un campione fermo, invece che l'audio che scorre.
namespace cleaner
{

enum class Preset
{
    ai,      // canzoni fatte con l'AI: fruscio metallico in alto e medi impastati
    mp3      // mp3 a basso bitrate: soprattutto il velo sopra i 10 kHz
};

// Pulisce il campione sul posto. Restituisce falso se non c'era niente da fare.
bool clean (juce::AudioBuffer<float>&, double sampleRate, Preset);

juce::String presetName (Preset);

} // namespace cleaner
