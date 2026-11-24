#pragma once

#include "control/ControlMap.h"
#include "engine/Chopper.h"
#include "engine/Compressor.h"
#include "engine/Cleaner.h"
#include "engine/Recorder.h"
#include "engine/Sequencer.h"
#include "engine/Slicer.h"
#include "engine/SampleEngine.h"
#include "machine/PadGrid.h"
#include "machine/StepGrid.h"

#include <array>
#include <functional>
#include <vector>

// La macchina: tiene lo stato e decide cosa fa ogni comando, senza sapere da
// dove arriva. Il Push, l'OSC e la finestra sul Mac le parlano tutti allo
// stesso modo, e lei risponde con le luci dei pad e le righe del display.
//
// I nomi dei modi sono quelli scritti sul pannello della macchina vera.
namespace machine
{

enum class PadLight { off, empty, loaded, selected, playing, muted };

class Machine
{
public:
    Machine();

    void prepare (double sampleRate);
    void process (juce::AudioBuffer<float>&);

    // --- quello che arriva dalle superfici
    void padDown (PadRef, int velocity);         // velocity 0..127
    void padUp (PadRef);
    void command (const control::Control&, bool down);
    void encoder (const control::Control&, int steps);

    // --- quello che le superfici fanno vedere
    PadLight lightFor (PadRef) const;
    sound::Kind kindFor (PadRef) const;

    // La pagina PASSO: la meta' di sopra della griglia scrive i passi del pad
    // scelto, quella di sotto resta pad.
    bool stepMode() const { return page == control::Command::pageStep; }
    int stepWindowStart() const { return stepOffset; }

    // Quello che serve per disegnare l'onda del campione sul display: i segni,
    // non i campioni. Chi disegna chiede il suono al campionatore una volta
    // sola, quando `sampleVersion` cambia.
    struct WaveMarkers
    {
        bool show = false;
        int drumIndex = 0;
        int sampleVersion = 0;
        double startFraction = 0.0;
        double endFraction = 1.0;
        double playFraction = -1.0;     // dove sta suonando, -1 se non suona
        std::vector<double> slices;
        sound::Kind kind = sound::Kind::generico;
        juce::String caption;
    };

    WaveMarkers waveMarkers() const;

    // Fa sentire il campione del pad scelto senza scriverlo nella sequenza:
    // serve per sentire il ritaglio mentre lo si muove.
    void auditionSelected();

    // Tira o accorcia il campione del pad perche' duri un numero intero di
    // battute al tempo di adesso, senza cambiargli il tono.
    void warpSelected();
    bool buttonLit (const control::Control&) const;

    // Come deve stare acceso un tasto. I tasti del trasporto, in basso a
    // destra del Push, hanno i colori di sempre: verde il play, rosso la
    // registrazione, bianco il resto.
    enum class Lamp { spento, smorto, acceso, lampeggia };

    struct ButtonLamp
    {
        Lamp state = Lamp::smorto;
        juce::Colour colour { 0xfff3f3f1 };
    };

    ButtonLamp lampFor (const control::Control&) const;
    juce::StringArray screenLines() const;
    // Le etichette degli otto tasti sotto il display, che cambiano con la
    // pagina. Una casella vuota vuol dire tasto spento.
    juce::StringArray softKeyLabels() const;
    juce::String lastMessage() const { return message; }

    // --- stato che serve anche fuori
    int bankGroup() const { return group; }
    void setBankGroup (int);
    PadRef selectedPad() const { return selected; }
    SampleEngine& engine() { return sampler; }
    Recorder& recorder() { return rec; }
    Sequencer& sequencer() { return seq; }
    Compressor& compressor() { return comp; }
    bool compressorOn() const { return compressing; }
    const Sequencer& sequencer() const { return seq; }
    const Recorder& recorder() const { return rec; }

    // Da chiamare ogni tanto dal filo dei messaggi: se il registratore ha
    // finito, il campione va sul pad scelto.
    bool checkRecorder();
    const SampleEngine& engine() const { return sampler; }
    double tempo() const { return bpm; }

    bool isFullLevel() const { return fullLevel; }
    bool isSixteenLevels() const { return sixteenLevels; }
    bool isMuteMode() const { return muteMode; }
    bool isMuted (PadRef p) const { return mutes[(size_t) drumIndex (p)]; }

    // Le fette di un brano, messe sui pad a blocchi di otto: ogni banco che
    // arriva dall'analisi parte da un pad multiplo di otto, cosi' la batteria
    // sta su una fila e la voce su quella dopo. Restituisce quanti pad ha
    // riempito.
    int placeSlices (const std::vector<chopper::Slice>&);

    // Le fette una dietro l'altra a partire dal pad scelto: e' quello che
    // serve per i mash up, un brano su un banco e un altro sul banco dopo.
    int placeFrom (PadRef start, const std::vector<chopper::Slice>&);

    // Taglia il campione che sta sul pad scelto e lo sparge sui pad che
    // seguono. Restituisce quanti pad ha riempito.
    int fillFromSelected();

    int sliceCount() const { return slices; }

    // Rimette sul pad il campione ripulito, tenendo da parte quello di prima
    // per il tasto ANNULLA.
    bool applyCleanedSample (int drumIndex, const juce::AudioBuffer<float>&,
                             double sourceRate, const juce::String& what);

    int selectedDrumIndex() const { return drumIndex (selected); }

    // Lo stato che non sta nei pad, quello che va salvato con la scena.
    struct State
    {
        double bpm = 120.0;
        int quantise = 3;
        int bankGroup = 0;
        int selectedPad = 0;        // indice del suono
        float masterGain = 0.8f;
        int slices = 16;
        std::array<bool, numDrums> mutes {};
    };

    State snapshot() const;
    void restore (const State&);

    // Svuota tutti i pad: serve prima di aprire una scena, cosi' non restano
    // in giro i suoni di quella di prima.
    void clearAllPads();

    // Il volume generale, come lo manda la striscia del Push.
    void setMasterGain (float);
    float masterVolume() const { return masterGain; }

    bool padsRecord() const { return padsIntoSequence; }

    void setMessageFromOutside (const juce::String& text) { setMessage (text); changed(); }

    std::function<void()> onChanged;          // qualcosa e' cambiato: ridisegna
    std::function<void()> onChopRequested;    // spacca un brano in fette
    std::function<void()> onFillRequested;    // chiedi un file lungo da tagliare a fette uguali
    std::function<void()> onSaveScene, onOpenScene, onToLibrary, onBrowseLibrary;
    std::function<void()> onSetup;             // apri la scheda audio
    std::function<void (int note, int velocity)> onPadPlayed;   // verso il MIDI di fuori
    std::function<void (cleaner::Preset)> onCleanRequested;  // pulisci il campione del pad

private:
    enum class LevelsType { volume, tune };

    void select (PadRef);
    void hit (PadRef, int velocity);
    void changeParameter (int which, int steps);
    void pushUndo (int drumIndex, const juce::String& what);
    void undo();
    void redo();
    void tapTempo();
    void moveSelection (int rows, int columns);
    void applyQuantiseToSequencer();
    int tickForStep (int step) const;
    bool stepPressed (int step);
    void setMessage (juce::String);
    void changed();

    SampleEngine sampler;
    Recorder rec;
    Sequencer seq;
    Compressor comp;
    bool compressing = false;
    int recordings = 0;
    int slices = 16;                    // in quante fette taglia RIEMPI
    bool soloMode = false;
    bool resampling = false;            // il registratore prende l'uscita, non l'ingresso
    bool padsIntoSequence = true;       // SUONA: i pad finiscono nella sequenza o no
    int stepOffset = 0;                 // da che passo comincia la finestra di 32

    PadRef selected { 0, 1 };
    int group = 0;                      // 0 = A..D, 1 = E..H
    control::Command page = control::Command::pageSample;
    juce::String pageName { "CAMPIONA" };
    int quantise = 3;                   // indice nella colonna, 1/16
    double bpm = 120.0;

    bool shift = false;
    bool fullLevel = false;
    bool sixteenLevels = false;
    LevelsType levelsType = LevelsType::volume;
    bool muteMode = false;
    bool eraseMode = false;
    bool noteRepeat = false;
    bool metronome = false;
    int copySource = -1;                // pad in attesa di essere copiato
    float masterGain = 0.8f;

    std::array<bool, numDrums> mutes {};
    std::array<bool, numDrums> held {};

    struct Snapshot { int drumIndex; Drum drum; juce::String what; };
    std::vector<Snapshot> undoStack, redoStack;

    // metronomo e note repeat vivono nel filo dell'audio
    double rate = 44100.0;
    double clickPhase = 0.0;
    int clickRemaining = 0;
    double samplesPerStep = 0.0;
    double stepCounter = 0.0;
    std::atomic<bool> repeatRunning { false };
    std::atomic<int> repeatDrum { -1 };
    std::atomic<int> repeatVelocity { 100 };
    std::atomic<bool> metronomeOn { false };
    std::atomic<double> tempoAtomic { 120.0 };

    juce::String message;
    juce::int64 lastTap = 0;
    std::vector<double> tapIntervals;
};

// Il valore di quantizzazione in frazioni di battuta: 1/4 vale 1.0
double quantiseBeats (int indexFromTop);

} // namespace machine
