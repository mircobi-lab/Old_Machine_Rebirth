#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// Il registratore: prende quello che entra dalla scheda (per adesso il
// microfono del Mac) e ne fa un campione da mettere su un pad.
//
// Funziona come quello della macchina vera: si arma, si decide la soglia, e la
// registrazione parte da sola quando il suono la supera. L'anticipo tiene da
// parte l'ultimo mezzo secondo prima dello scatto, cosi' l'attacco non si
// perde mai: senza quello, quando la soglia scatta il colpo e' gia' passato.
//
// Il filo dell'audio qui dentro non alloca e non aspetta niente: scrive in un
// pezzo di memoria preso una volta per tutte e muove degli atomici. Chi guarda
// (la finestra, il display) legge quando vuole.
class Recorder
{
public:
    enum class State { fermo, armato, registra, pieno };

    void prepare (double sampleRate, int numChannels);

    // Dal filo dell'audio, con quello che e' appena entrato.
    void processInput (const float* const* input, int numChannels, int numSamples);

    void arm();                      // aspetta che il suono superi la soglia
    void startNow();                 // parte subito
    void stop();                     // ferma e tiene quello che ha preso
    void cancel();                   // butta via tutto

    State state() const { return current.load(); }
    bool isWaiting() const { return current.load() == State::armato; }
    bool isRecording() const { return current.load() == State::registra; }
    bool hasTake() const { return current.load() == State::pieno; }

    // Il campione registrato. Lo si prende una volta sola: dopo, il
    // registratore e' pronto per un altro giro.
    bool takeRecording (juce::AudioBuffer<float>& destination);

    float inputLevel() const { return level.load(); }     // 0..1, per la barra
    double recordedSeconds() const;

    // --- i tre numeri che si girano
    void setThreshold (int percent);                 // 0..99, 0 = parte subito
    void setLengthSeconds (double);                  // 1..60
    void setPreRollSeconds (double);                 // 0..1

    int threshold() const { return thresholdPercent; }
    double lengthSeconds() const { return takeLength; }
    double preRollSeconds() const { return preRoll; }

    bool monitoring() const { return monitor.load(); }
    void setMonitoring (bool on) { monitor.store (on); }

    static constexpr double maxLengthSeconds = 60.0;
    static constexpr double maxPreRollSeconds = 1.0;

private:
    void beginWriting();
    float thresholdLinear() const;

    double rate = 44100.0;
    int channels = 1;

    juce::AudioBuffer<float> take;       // dove finisce la registrazione
    juce::AudioBuffer<float> pre;        // l'anello dell'anticipo
    int preWrite = 0;
    int preFilled = 0;

    std::atomic<State> current { State::fermo };
    std::atomic<int> written { 0 };      // quanti campioni ci sono nel take
    std::atomic<float> level { 0.0f };
    std::atomic<bool> monitor { false };

    int thresholdPercent = 8;
    double takeLength = 10.0;
    double preRoll = 0.3;
};
