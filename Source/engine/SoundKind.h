#pragma once

#include <juce_graphics/juce_graphics.h>

// A che famiglia appartiene un suono, e di che colore si accende il suo pad.
//
// I colori stanno nelle tinte dello stile MBStudio: blu, celeste, verde scuro
// e grigi, niente arancioni o viola.
//
// La batteria e' divisa per strumento e va dal blu piu' cupo all'azzurro piu'
// chiaro salendo di tono: cassa, tom, rullante, percussioni, piatti. Cosi'
// guardando la griglia si legge il kit senza leggere il display, e i piatti
// non si confondono con la cassa.
namespace sound
{

enum class Kind
{
    generico,
    cassa,          // kick
    rullante,       // snare, rim, battimani
    piatti,         // charleston, crash, ride
    tom,
    percussioni,
    voce,
    giro,
    accordi,
    basso,
    registrato
};

constexpr int numKinds = 11;

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
