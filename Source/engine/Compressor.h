#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Il compressore sull'uscita generale, come il COMPRESSOR della macchina vera.
//
// E' quello classico a rilevazione di picco, con i due canali legati insieme:
// se tira giu' lo fa su tutti e due, altrimenti l'immagine stereo si sposta a
// ogni colpo di cassa.
class Compressor
{
public:
    void prepare (double sampleRate);
    void process (juce::AudioBuffer<float>&);

    void setThresholdDb (double);      // -40 .. 0
    void setRatio (double);            // 1 .. 20
    void setAttackMs (double);         // 1 .. 100
    void setReleaseMs (double);        // 20 .. 1000

    double thresholdDb() const { return threshold; }
    double ratio() const { return amount; }
    double attackMs() const { return attack; }
    double releaseMs() const { return release; }

    // Di quanti dB sta tirando giu' adesso: per la barra sul display.
    float gainReductionDb() const { return reduction; }

private:
    void updateCoefficients();

    double rate = 44100.0;
    double threshold = -12.0;
    double amount = 4.0;
    double attack = 10.0;
    double release = 200.0;

    double attackCoeff = 0.0, releaseCoeff = 0.0;
    double envelope = 0.0;             // in dB sopra la soglia
    float reduction = 0.0f;
};
