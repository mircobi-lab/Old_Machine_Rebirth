#pragma once

#include "engine/SampleEngine.h"

#include <array>
#include <atomic>

// Il sequencer: registra quello che si batte sui pad e lo rifa a tempo.
//
// Conta come la macchina vera, 96 passi per battuta (quarto), e tiene 99
// sequenze. Ogni sequenza e' un pezzo di memoria preso in partenza, cosi' non
// si alloca mai mentre si suona.
//
// Chi scrive e chi legge stanno su fili diversi: i colpi arrivano dal Push o
// dalla finestra, la riproduzione gira nel filo dell'audio. Per questo i colpi
// si aggiungono solo in fondo alla lista, con il numero di eventi che diventa
// vero per ultimo: il filo dell'audio legge fino a quel numero e quello che c'e'
// oltre non lo guarda nemmeno. Le modifiche che spostano o togliono roba
// alzano una bandiera e il filo dell'audio salta quel blocco: un buco di pochi
// millesimi che non si sente.
class Sequencer
{
public:
    static constexpr int ticksPerBeat = 96;
    static constexpr int beatsPerBar = 4;
    static constexpr int ticksPerBar = ticksPerBeat * beatsPerBar;
    static constexpr int numSequences = 99;
    static constexpr int maxEvents = 2048;
    static constexpr int maxBars = 64;

    struct Event
    {
        int tick = 0;
        int drum = 0;
        int velocity = 0;
    };

    void prepare (double sampleRate);

    // --- filo dell'audio
    void process (int numSamples, double bpm, SampleEngine&);

    // --- trasporto, dal filo dei comandi
    void play();
    void stop();                     // la seconda volta torna all'inizio
    void toggleRecord();             // arma la registrazione
    void toggleOverdub();
    void rewind();

    bool isPlaying() const { return playing.load(); }
    bool isRecording() const { return recording.load(); }
    bool isOverdub() const { return overdub.load(); }

    // --- scrivere
    void recordHit (int drum, int velocity);
    void eraseDrum (int drum);       // toglie tutti i colpi di quel pad
    void clearSequence();
    void newSequence();              // la prima sequenza vuota che trova
    // Copia la sequenza di adesso in un'altra, battute comprese.
    void copyTo (int destination);

    void doubleLength();             // raddoppia le battute copiando i colpi
    void halveLength();

    // --- stato
    int sequenceIndex() const { return current; }
    void setSequence (int index);
    int bars() const;
    void setBars (int);
    int eventCount() const;
    int eventCount (int sequence) const;

    double positionInBeats() const;
    int positionBar() const;
    int positionBeat() const;        // 1..4

    void setTimingCorrect (bool on) { correct.store (on); }
    bool timingCorrect() const { return correct.load(); }
    void setQuantiseTicks (int);     // a quanto si incolla il colpo
    int quantiseTicks() const { return quantise; }

    void setSwing (int percent);     // 50 = niente, 75 = tanto
    int swing() const { return swingPercent; }

    // Quale pad ha suonato in questo giro: per accendere i pad che vanno
    int lastTriggered() const { return triggered.load(); }

private:
    struct Sequence
    {
        std::array<Event, maxEvents> events {};
        std::atomic<int> count { 0 };
        std::atomic<int> barCount { 2 };
    };

    int lengthInTicks() const;
    int quantised (int tick) const;
    int swungTick (int tick) const;
    void fire (SampleEngine&, int fromTick, int toTick);

    std::array<Sequence, numSequences> sequences;
    int current = 0;

    double rate = 44100.0;
    double ticks = 0.0;                      // posizione, in passi
    std::atomic<double> shownTicks { 0.0 };
    std::atomic<bool> playing { false };
    std::atomic<bool> recording { false };
    std::atomic<bool> overdub { false };
    std::atomic<bool> editing { false };
    std::atomic<bool> correct { true };
    std::atomic<int> triggered { -1 };
    std::atomic<bool> stopPressedOnce { false };

    int quantise = ticksPerBeat / 4;         // un sedicesimo
    int swingPercent = 50;
};
