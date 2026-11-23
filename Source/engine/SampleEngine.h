#pragma once

#include "engine/Drum.h"
#include "machine/PadGrid.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>

// Il campionatore: 32 voci stereo, i suoni in memoria, l'accordatura fatta
// rileggendo il campione a passo variabile.
//
// I pad vengono battuti dal filo dei messaggi o da quello del MIDI, il suono si
// fa nel filo dell'audio: in mezzo c'e' una coda senza serrature, cosi' il filo
// dell'audio non aspetta mai nessuno.
class SampleEngine
{
public:
    static constexpr int numVoices = 32;

    SampleEngine();

    void prepare (double sampleRate);
    void process (juce::AudioBuffer<float>&);

    void trigger (int drumIndex, float velocity);     // velocity 0..1
    void stopAll();

    Drum& drum (int index);
    const Drum& drum (int index) const;
    int loadedCount() const;
    int activeVoices() const { return playing.load(); }

    // Carica un file sul pad. Vero se c'e' riuscito.
    bool loadFile (int drumIndex, const juce::File&);

    // Mette sul pad un suono che e' gia' in memoria: le fette tagliate da un
    // brano e i campioni ripuliti passano da qui, senza scrivere niente su
    // disco.
    bool loadBuffer (int drumIndex, const juce::String& name,
                     const juce::AudioBuffer<float>&, double sourceRate,
                     sound::Kind = sound::Kind::generico);

    // Il campione del pad, copiato fuori per poterlo lavorare in pace.
    juce::AudioBuffer<float> copyOf (int drumIndex) const;

    // Otto suoni fatti in casa sui primi pad, cosi' la macchina suona subito
    void loadStarterKit();

private:
    struct Voice
    {
        int drum = -1;
        double position = 0.0;
        double increment = 1.0;
        float gain = 0.0f;
        std::uint32_t startedAt = 0;
        bool active = false;
    };

    struct Hit { int drum = 0; float velocity = 0.0f; };

    void startVoice (int drumIndex, float velocity);
    double incrementFor (const Drum&) const;

    std::array<Drum, machine::numDrums> drums;
    std::array<Voice, numVoices> voices;

    // la coda dei colpi in arrivo
    static constexpr int queueSize = 256;
    juce::AbstractFifo fifo { queueSize };
    std::array<Hit, queueSize> queue;

    mutable juce::CriticalSection loadLock;
    juce::AudioFormatManager formats;
    double rate = 44100.0;
    std::uint32_t counter = 0;
    std::atomic<int> playing { 0 };
};
