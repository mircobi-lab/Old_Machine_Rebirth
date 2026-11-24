#pragma once

#include "engine/SampleEngine.h"

#include <array>
#include <atomic>
#include <memory>

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
    void setCountInBars (int);       // 0, 1 o 2 battute di avvio
    int countInBars() const { return countIn; }
    bool isCountingIn() const { return counting.load(); }
    void toggleOverdub();
    void rewind();

    bool isPlaying() const { return playing.load(); }
    bool isRecording() const { return recording.load(); }
    bool isOverdub() const { return overdub.load(); }

    // --- scrivere
    void recordHit (int drum, int velocity);
    void eraseDrum (int drum);       // toglie tutti i colpi di quel pad

    // --- scrivere i passi a mano, nella pagina PASSO
    // Il colpo si cerca e si scrive in una finestrella intorno al passo, non
    // sul singolo tick: un colpo suonato a mano cade sempre un po' prima o un
    // po' dopo, e deve comunque accendere il suo passo.
    bool hasHit (int drum, int tick, int window) const;
    void setHit (int drum, int tick, int velocity);
    void removeHit (int drum, int tick, int window);
    bool toggleHit (int drum, int tick, int window, int velocity);

    int velocityAt (int drum, int tick, int window) const;
    int length() const { return lengthInTicks(); }
    int positionTicks() const { return (int) shownTicks.load(); }
    void clearSequence();
    void newSequence();              // la prima sequenza vuota che trova
    // Copia la sequenza di adesso in un'altra, battute comprese.
    void copyTo (int destination);

    void doubleLength();             // raddoppia le battute copiando i colpi
    void halveLength();

    // --- il brano: le sequenze una dietro l'altra
    struct SongStep { int sequence = 0; int repeats = 1; };
    static constexpr int maxSongSteps = 64;

    void addToSong (int sequence, int repeats = 1);
    void removeLastSongStep();
    void clearSong();
    int songLength() const { return songSteps; }
    SongStep songStep (int index) const;
    int songPosition() const { return songAt.load(); }

    void setSongMode (bool);
    bool songMode() const { return song.load(); }

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

    // La sequenza a meta' o a doppia velocita', come HALF SPEED e DOUBLE
    // SPEED sulla macchina vera: il tempo resta quello, cambia il passo.
    void setSpeed (double);
    double speed() const { return speedFactor; }

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

    // Le 99 sequenze stanno nel mucchio, non dentro l'oggetto: in tutto sono
    // un paio di megabyte, e una macchina creata sulla pila li farebbe
    // scoppiare (su Windows la pila di un filo e' un megabyte). La memoria si
    // prende una volta sola, quando la macchina nasce, quindi il filo
    // dell'audio continua a non allocare niente.
    std::unique_ptr<Sequence[]> sequences { new Sequence[numSequences] };
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
    double speedFactor = 1.0;

    int countIn = 0;                         // battute prima di partire
    std::atomic<bool> counting { false };
    double countTicks = 0.0;

    std::array<SongStep, maxSongSteps> songList {};
    int songSteps = 0;
    std::atomic<bool> song { false };
    std::atomic<int> songAt { 0 };
    int songRepeat = 0;                      // a che giro sta il passo di adesso
};
