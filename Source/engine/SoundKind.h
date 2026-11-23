#pragma once

#include <juce_graphics/juce_graphics.h>

// A che famiglia appartiene un suono, e di che colore si accende il suo pad.
//
// I colori stanno nelle tinte dello stile MBStudio: grigi, blu, celeste e
// verde scuro, niente arancioni o viola. Cosi' guardando la griglia si capisce
// subito dove sta la batteria e dove stanno le voci, senza leggere il display.
namespace sound
{

enum class Kind
{
    generico,
    batteria,
    percussioni,
    voce,
    giro,
    accordi,
    basso,
    registrato
};

constexpr int numKinds = 8;

Kind kindFromIndex (int);
int indexOf (Kind);

// Il nome della famiglia, in italiano: da passare a lang::t per il display.
juce::String name (Kind);

// Il colore del pad quando il suono c'e' ed e' fermo.
juce::Colour colour (Kind);

// Lo stesso colore nei vari stati del pad.
juce::Colour colourSelected (Kind);
juce::Colour colourPlaying (Kind);

// Indovina la famiglia dal nome del campione o del banco: serve quando il
// suono arriva da un file che l'utente ha scelto a mano.
Kind guessFromName (const juce::String&);

} // namespace sound
