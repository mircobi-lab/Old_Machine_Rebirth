#pragma once

#include "machine/Machine.h"

// I file MIDI: si caricano dentro una sequenza e si rimandano fuori.
//
// Le note vanno sui pad col conto delle macchine vere: la nota 35 e' il primo
// pad, la 98 e' il sessantaquattresimo. Un file uscito da una di quelle
// macchine ritrova i suoi pad senza che si tocchi niente.
namespace midifile
{

struct Result
{
    int notes = 0;
    int bars = 0;
    double bpm = 0.0;
    juce::String error;

    bool ok() const { return error.isEmpty() && notes > 0; }
};

// Mette il file dentro la sequenza di adesso, cancellando quello che c'era.
Result import (const juce::File&, machine::Machine&);

// Scrive la sequenza di adesso in un file MIDI.
bool exportSequence (const juce::File&, machine::Machine&);

// Da nota a pad e viceversa, il conto delle macchine vere.
int drumForNote (int note);
int noteForDrum (int drum);

} // namespace midifile
