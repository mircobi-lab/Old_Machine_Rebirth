#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include <atomic>
#include <vector>

struct libusb_context;
struct libusb_device_handle;

// Manda al display del Push 2 l'ultima immagine ricevuta, circa 25 volte al secondo.
// Il Push spegne il display se non riceve frame per 2 secondi, quindi il thread
// rimanda lo stesso frame anche quando non cambia.
//
// Se il Push non c'e', prova il simulatore push-simulator, che ascolta su
// 127.0.0.1:48484 e si aspetta lo stesso frame preceduto da "PSMF" e dalla
// lunghezza su 4 byte. Li' il frame si manda solo quando cambia: non serve
// rinfrescarlo come sul display vero.
class Push2Display final : private juce::Thread
{
public:
    enum class State { searching, busy, connected, simulator };

    static constexpr int simulatorPort = 48484;

    Push2Display();
    ~Push2Display() override;

    void setImage (const juce::Image& image); // 960x160, qualsiasi formato
    State getState() const noexcept { return state.load(); }
    juce::String getLastError() const;

private:
    void run() override;
    bool open();
    void close();
    bool sendFrame (const std::vector<std::uint8_t>& frame);
    bool sendFrameToSimulator (const std::vector<std::uint8_t>& frame);
    bool openSimulator();

    libusb_context* context = nullptr;
    libusb_device_handle* handle = nullptr;
    juce::StreamingSocket socket;

    juce::CriticalSection lock;
    std::vector<std::uint8_t> pending; // frame gia' codificato
    juce::String lastError;
    std::atomic<State> state { State::searching };
};
