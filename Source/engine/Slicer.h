#pragma once

#include "engine/Chopper.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Taglia un campione lungo in fette uguali, per riempire tanti pad in un colpo
// e fare i mash up: un brano intero su 64 pad e poi si suona a pezzi.
//
// Non e' il taglio furbo di SPACCA, che cerca i colpi buoni: qui si divide e
// basta, perche' su un brano a tempo le fette uguali cadono sulle battute e si
// sa sempre dove mettere le mani.
namespace slicer
{

// Quante fette si possono chiedere, nell'ordine in cui girano col tasto.
const std::vector<int>& counts();
int nextCount (int current, int direction);

// Taglia in fette uguali. I tagli vengono spostati sul passaggio per lo zero
// piu' vicino e ogni fetta ha un millisecondo di dissolvenza ai capi: senza
// questo, a ogni attacco si sente il clic.
std::vector<chopper::Slice> cut (const juce::AudioBuffer<float>&, double sampleRate,
                                 int pieces, const juce::String& baseName,
                                 const juce::String& bank = "FETTE",
                                 sound::Kind = sound::Kind::giro);

} // namespace slicer
