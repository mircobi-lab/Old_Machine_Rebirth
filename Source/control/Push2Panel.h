#pragma once

#include "machine/Machine.h"
#include "control/ControlMap.h"
#include "push2/Push2Display.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>
#include <optional>
#include <map>

// Il Push 2 come pannello della macchina: pad, tasti, encoder e display.
//
// Lavora sulla porta User e ci mette il Push da solo col sysex 0A 01, lasciando
// libera la porta Live per Ableton; all'uscita lo rimette in Live mode. I colori
// dei pad sono nostri: si scrivono sei voci della tavolozza del Push dopo essersi
// segnati quelle originali, che vengono rimesse quando si chiude.
class Push2Panel final : private juce::MidiInputCallback,
                         private juce::Timer
{
public:
    explicit Push2Panel (juce::Typeface::Ptr displayFont);
    ~Push2Panel() override;

    bool isConnected() const { return input != nullptr && output != nullptr; }
    juce::String portDescription() const { return portName; }

    void setPadLight (machine::PadRef, machine::PadLight);
    void setBankGroup (int group);                  // 0 = A..D, 1 = E..H
    int getBankGroup() const { return bankGroup; }
    void setScreenLines (juce::StringArray);        // cosa scrivere sul display
    juce::Image lastScreen() const;                 // l'ultima schermata, per rivederla sul Mac

    std::function<void (machine::PadRef, int velocity)> onPadDown;
    std::function<void (machine::PadRef)> onPadUp;
    std::function<void (int group)> onBankGroupChanged;
    std::function<void (const control::Control&, bool down)> onCommand;
    std::function<void (const control::Control&, int delta)> onEncoder;

    // Luce di un tasto: per i bianchi 0..127, per gli RGB l'indice della tavolozza
    void setButtonLight (int cc, int value);

    static std::optional<juce::MidiDeviceInfo> choosePort (const juce::Array<juce::MidiDeviceInfo>&,
                                                           bool preferUserPort);
    static int paletteIndexFor (machine::PadLight);

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;
    void reconnect();
    void send (const std::vector<juce::MidiMessage>&);
    void sendPalette();
    void restorePalette();
    void refreshLeds (bool everything);
    void lightAllButtons();
    void drawScreen();
    static juce::MidiMessage sysexFor (const std::vector<std::uint8_t>&);

    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::MidiDeviceListConnection devicesChanged;
    std::unique_ptr<Push2Display> display;
    juce::Typeface::Ptr font;

    juce::String portName;
    int bankGroup = 0;
    std::array<machine::PadLight, machine::numDrums> lights {};
    std::array<machine::PadLight, 64> onScreen {};           // cosa c'e' davvero sui LED
    bool fullRefresh = true;
    juce::StringArray screenLines;
    juce::Image screenImage;
    juce::CriticalSection lock;
    std::vector<juce::MidiMessage> originalPalette;
    std::map<int, int> buttonLights;      // cc -> valore mandato
};
