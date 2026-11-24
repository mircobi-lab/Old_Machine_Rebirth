#pragma once

#include "machine/Machine.h"

// Legge un programma della macchina vera, il file .xpm, e mette i campioni sui
// nostri pad.
//
// Il programma e' un file XML: dentro ci sono gli strumenti, uno per pad, e
// ogni strumento ha i suoi strati con il nome del campione, piu' i parametri
// del pad. Il campione vero sta in un wav li' accanto, di solito nella stessa
// cartella o in una sottocartella Samples.
//
// I nomi dei campi cambiano da una versione all'altra della macchina, quindi
// il lettore prende quello che riconosce e lascia stare il resto invece di
// rifiutare il file: un kit si carica anche se un parametro ha un altro nome.
namespace mpc
{

struct Result
{
    juce::String programName;
    int padsLoaded = 0;
    juce::StringArray missing;      // i campioni nominati ma non trovati
    juce::String error;

    bool ok() const { return error.isEmpty() && padsLoaded > 0; }
};

Result loadProgram (const juce::File& xpm, machine::Machine&);

// Dove cerchiamo il wav di un campione nominato dal programma.
juce::File findSample (const juce::File& programFolder, const juce::String& name);

} // namespace mpc
