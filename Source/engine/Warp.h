#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// WARP: cambia la durata di un campione senza cambiarne il tono.
//
// Tirare o accorciare un suono cambiando la velocita' di lettura fa anche
// salire o scendere il tono, e una voce diventa un fumetto. Qui invece il
// campione viene tagliato a pezzetti che si sovrappongono e rincollato piu'
// lungo o piu' corto, cercando ogni volta il punto in cui i due pezzi si
// somigliano di piu': cosi' la durata cambia e il tono resta dov'era. E' il
// mestiere che fanno tutte le macchine che "warpano".
namespace warp
{

// fattore > 1 allunga (piu' lento), < 1 accorcia. Fuori da 0.25 - 4 non si va.
juce::AudioBuffer<float> stretch (const juce::AudioBuffer<float>&, double factor);

// Il fattore che serve perche' il campione duri un numero intero di battute a
// quel tempo. `bars` dice quante battute sono venute fuori.
double factorForTempo (int lengthInSamples, double sampleRate, double bpm, int& bars);

} // namespace warp
