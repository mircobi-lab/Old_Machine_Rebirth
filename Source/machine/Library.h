#pragma once

#include "machine/Machine.h"

// Le scene e la libreria.
//
// Una scena e' tutta la macchina come sta in quel momento: i suoni sui 128
// pad, i loro parametri, il tempo, il passo, i mute. Sta in una cartella con
// un file di testo che descrive il tutto e un wav per ogni pad pieno: cosi' i
// campioni si possono anche riusare a mano, invece di restare chiusi dentro un
// formato nostro.
//
// La libreria e' una cartella di wav con un nome: ogni campione registrato o
// caricato ci si puo' mettere, e da li' torna su qualunque pad.
namespace library
{

juce::File root();              // la cartella di tutto, dentro la musica dell'utente
juce::File scenesFolder();
juce::File samplesFolder();

// I nomi delle scene salvate, in ordine.
juce::StringArray sceneNames();
juce::File sceneFolder (const juce::String& name);

// Salva e apre. `error` resta vuoto se e' andata bene.
bool saveScene (const juce::File& folder, const machine::Machine&, juce::String& error);
bool loadScene (const juce::File& folder, machine::Machine&, juce::String& error);

// Mette un campione nella libreria e restituisce il file scritto.
juce::File addToLibrary (const juce::File& folder, const juce::String& name,
                         const juce::AudioBuffer<float>&, double sampleRate);

// Quanti campioni ci sono nella libreria.
int librarySize (const juce::File& folder);

} // namespace library
