#pragma once

#include "control/ControlMap.h"
#include "engine/SampleEngine.h"
#include "machine/PadGrid.h"

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
    bool buttonLit (const control::Control&) const;
    juce::StringArray screenLines() const;
    juce::String lastMessage() const { return message; }

    // --- stato che serve anche fuori
    int bankGroup() const { return group; }
    void setBankGroup (int);
    PadRef selectedPad() const { return selected; }
    SampleEngine& engine() { return sampler; }
    const SampleEngine& engine() const { return sampler; }
    double tempo() const { return bpm; }

    bool isFullLevel() const { return fullLevel; }
    bool isSixteenLevels() const { return sixteenLevels; }
    bool isMuteMode() const { return muteMode; }
    bool isMuted (PadRef p) const { return mutes[(size_t) drumIndex (p)]; }

    std::function<void()> onChanged;    // qualcosa e' cambiato: ridisegna

private:
    enum class LevelsType { volume, tune };

    void select (PadRef);
    void hit (PadRef, int velocity);
    void changeParameter (int which, int steps);
    void pushUndo (int drumIndex, const juce::String& what);
    void undo();
    void redo();
    void tapTempo();
    void setMessage (juce::String);
    void changed();

    SampleEngine sampler;

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
