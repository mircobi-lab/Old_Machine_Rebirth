#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace mbstudio
{

// L'onda del campione da far vedere sul display, con sopra i segni che servono
// per lavorarci: inizio e fine del ritaglio, e i tagli che le fette farebbero
// se si premesse RIEMPI adesso.
//
// I numeri sono frazioni da 0 a 1 della lunghezza del campione, cosi' chi
// disegna non deve sapere niente di campioni e di frequenze.
struct WaveView
{
    std::vector<float> lows, highs;       // una coppia per colonna di pixel
    double startFraction = 0.0;
    double endFraction = 1.0;
    std::vector<double> slices;           // i tagli proposti
    double playFraction = -1.0;           // dove sta suonando, -1 se non suona
    juce::Colour colour { 0xff3d8bff };   // il colore della famiglia del suono
    juce::String caption;                 // scritta piccola in un angolo

    bool isEmpty() const { return lows.empty(); }
};

// Riduce il campione a una coppia di valori per colonna: il minimo e il massimo
// di quel pezzo. E' la stessa cosa che fa ogni editor audio, e si fa una volta
// sola per campione, non a ogni disegno.
WaveView computePeaks (const juce::AudioBuffer<float>&, int columns);

// Disegna l'onda nel rettangolo dato.
void drawWave (juce::Graphics&, juce::Rectangle<int>, const WaveView&,
               juce::Typeface::Ptr font);

} // namespace mbstudio
