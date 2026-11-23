#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// Che cosa fa ogni tasto del Push 2 su questa macchina.
//
// Il Push ha 65 tasti piu' gli 11 rotativi e la striscia, e qui si usano tutti
// quelli che hanno un senso: i numeri vengono dalla mappa ufficiale di Ableton
// (doc/Push2-map.json e MidiMapping.png di Ableton/push-interface).
namespace control
{

enum class Command
{
    none,

    // Le otto pagine, sui tasti sopra il display
    pageSample, pageChop, pageSound, pageSequence, pageStep, pageSong, pageMix, pageDisk,

    // I tasti sotto il display: cambiano con la pagina
    softKey,

    // Trasporto e registrazione
    play, stop, recordSequence, overdub, recordSample, newSequence, duplicate, fixedLength,

    // Modifica
    erase, undo, timingCorrect, doubleLength, resample,

    // Come suonano i pad
    noteRepeat, fullLevel, sixteenLevels, padMute, padSolo, select, shift,
    layout, playMode, sequenceMode, tuneUp, tuneDown,

    // Spostarsi
    up, down, left, right, pageLeft, pageRight,

    // Tempo e servizio
    tapTempo, metronome, setup, handBackToLive, master,

    // Pagine di servizio sul fianco destro del display
    padParameters, drumMix, browse, sequenceList, addEffect, addTrack,

    // La colonna a destra dei pad: il valore della quantizzazione, 1/4 ... 1/32t
    quantiseValue,

    // I rotativi
    parameter,        // gli otto in fila, indice 0..7
    mainVolume,       // il master
    tempo,            // il primo a sinistra
    swing             // il secondo a sinistra
};

struct Control
{
    int number = 0;              // numero di CC per i tasti, di CC anche per i rotativi
    Command command = Command::none;
    int index = 0;               // per i gruppi: quale tasto della fila
    juce::String label;          // come si chiama sulla macchina, non sul Push
    bool rgb = false;            // il LED e' a colori o solo bianco
};

// Tutti i comandi, nell'ordine del pannello.
const std::vector<Control>& buttons();
const std::vector<Control>& encoders();

// Il comando di un tasto, cercato per numero di CC. command == none se quel
// tasto non lo usiamo.
Control forButton (int cc);
Control forEncoder (int cc);

// Il valore di quantizzazione della colonna a destra dei pad, dall'alto in
// basso: 1/32t, 1/32, 1/16t, 1/16, 1/8t, 1/8, 1/4t, 1/4.
juce::String quantiseName (int indexFromTop);

} // namespace control
