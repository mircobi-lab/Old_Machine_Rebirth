#pragma once

#include "control/ControlMap.h"
#include "machine/PadGrid.h"

#include <juce_osc/juce_osc.h>
#include <functional>

// La macchina senza il Push 2.
//
// Parla OSC con la stessa lingua del pannello vero: gli stessi numeri di CC per
// i tasti, gli stessi banchi e pad, gli stessi parametri. Cosi' chi il Push non
// ce l'ha suona lo stesso, da un tablet o da un'altra finestra, e chi ce l'ha
// puo' usare tutti e due insieme.
//
// Indirizzi che riceve:
//   /pad      banco(1..8) pad(1..16) velocity(0..127)   0 = lasciato
//   /button   cc valore(0 o 1)
//   /encoder  indice(1..8) passi            passi negativi per girare indietro
//   /tempo /swing /volume   passi
//
// Indirizzi che manda:
//   /led/pad     banco pad stato("off","empty","loaded","selected","playing")
//   /led/button  cc valore(0..127)
//   /screen/title testo
//   /screen/field indice(1..8) etichetta valore
class OscSurface final : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    OscSurface();
    ~OscSurface() override;

    // Apre la porta su cui si ascolta e quella a cui si risponde.
    bool start (int receivePort = 9000, juce::String sendHost = "127.0.0.1", int sendPort = 9001);
    void stop();

    bool isListening() const { return listening; }
    juce::String description() const;

    void sendPadLight (machine::PadRef, const juce::String& state);
    void sendButtonLight (int cc, int value);
    void sendScreen (const juce::StringArray& lines);   // come Push2Panel: "#titolo" e "ETICHETTA|VALORE"

    std::function<void (machine::PadRef, int velocity)> onPadDown;
    std::function<void (machine::PadRef)> onPadUp;
    std::function<void (const control::Control&, bool down)> onCommand;
    std::function<void (const control::Control&, int delta)> onEncoder;

private:
    void oscMessageReceived (const juce::OSCMessage&) override;
    static int intArg (const juce::OSCMessage&, int index, int fallback = 0);

    juce::OSCReceiver receiver;
    juce::OSCSender sender;
    bool listening = false, sending = false;
    int inPort = 0, outPort = 0;
    juce::String host;
};
