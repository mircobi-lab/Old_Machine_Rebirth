#pragma once

#include "control/OscSurface.h"
#include "control/Push2Panel.h"
#include "machine/Library.h"
#include "machine/Machine.h"
#include "ui/Lang.h"
#include "ui/MBStudioLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <optional>

// La finestra sul Mac: fa vedere in grande quello che c'e' sul Push e serve per
// la scheda audio e per caricare i suoni. Il pannello vero resta il Push, o il
// telefono quando il Push non c'e'.
class MainComponent final : public juce::AudioAppComponent,
                            public juce::FileDragAndDropTarget,
                            private juce::MidiInputCallback,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

private:
    void timerCallback() override;
    void refreshSurfaces();
    void refreshLights();
    void refreshWave();

    // MIDI CONFIG: i pad si suonano anche da fuori, e quello che si batte
    // esce verso le altre macchine.
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void sendMidiOut (const juce::MidiMessage&);
    juce::Rectangle<int> cellBounds (int row, int column) const;
    std::optional<machine::PadRef> padAt (juce::Point<int>) const;
    void loadOntoSelected();
    void chooseFileToChop();
    void runChop (const juce::File&);
    void runClean (cleaner::Preset);
    void chooseFileToFill();
    void loadDropped (const juce::File&, machine::PadRef& target);
    void loadProgram (const juce::File&);
    void loadFolder (const juce::File&, machine::PadRef& target);
    void askSceneName();
    void saveSceneAs (const juce::String& name);
    void chooseSceneToOpen();
    void putSelectedInLibrary();
    void browseLibrary();
    void setBusy (juce::String);
    juce::String busyMessage() const;
    void showOscSettings();
    void startOsc();
    static juce::File oscSettingsFile();
    static juce::File languageFile();
    void loadLanguage();
    void cycleLanguage();
    void retranslate();

    machine::Machine mpc;

    MBStudioBrand brand;
    juce::Rectangle<int> brandArea, gridArea, debossArea, screenArea;
    juce::Label status;
    juce::TextButton audioButton { "AUDIO" }, bankButton { "BANCHI A-D" },
                     oscButton { "OSC" }, loadButton { "CARICA" },
                     chopButton { "SPACCA" }, cleanButton { "PULISCI" },
                     langButton { "ITA" }, sceneButton { "SCENA" };

    // L'analisi di un brano intero dura: va fatta da parte, non nel filo dei
    // messaggi, altrimenti la macchina resta ferma mentre lavora.
    juce::ThreadPool pool { 1 };
    mutable juce::CriticalSection busyLock;
    juce::String busy;

    juce::AudioBuffer<float> inputMix;    // per l'ascolto dell'ingresso

    // L'onda si ricalcola solo quando cambia il campione, non a ogni disegno:
    // su un brano di un minuto sono due milioni e mezzo di campioni da
    // scorrere, e il display si ridisegna venti volte al secondo.
    mbstudio::WaveView wavePeaks;
    int wavePad = -1, waveVersion = -1, waveLength = -1;

    std::unique_ptr<Push2Panel> push;
    std::unique_ptr<OscSurface> osc;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> prompt;
    std::optional<machine::PadRef> mousePad;
};
