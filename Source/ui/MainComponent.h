#pragma once

#include "control/Push2Panel.h"
#include "control/OscSurface.h"
#include "machine/PadGrid.h"
#include "ui/MBStudioLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>

// La finestra sul Mac: fa vedere in grande quello che c'e' sul Push, e serve per
// scegliere la scheda audio. Il pannello vero resta il Push.
class MainComponent final : public juce::AudioAppComponent,
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

private:
    void timerCallback() override;
    void hit (machine::PadRef, int velocity);
    juce::Rectangle<int> cellBounds (int row, int column) const;
    void updateScreen();

    MBStudioBrand brand;
    juce::Rectangle<int> brandArea, gridArea, debossArea, screenArea;
    juce::Label status;
    juce::TextButton audioButton { "AUDIO" }, bankButton { "BANCHI A-D" }, oscButton { "OSC" };

    std::unique_ptr<Push2Panel> push;
    std::unique_ptr<OscSurface> osc;
    machine::PadRef selected { 0, 1 };
    control::Command page = control::Command::pageSample;
    juce::String pageName { "CAMPIONA" }, lastCommand;
    int quantise = 3;        // 1/16, come l'MPC appena accesa
    void command (const control::Control&, bool down);
    void refreshButtonLights();
    void padLight (machine::PadRef, Push2Panel::PadLight);
    void buttonLight (int cc, int value, bool rgb);
    void showOscSettings();
    static juce::File oscSettingsFile();
    void startOsc();

    // Per ora ogni pad fa un colpo secco: serve a sentire che la catena audio va
    struct Click { std::atomic<int> remaining { 0 }; std::atomic<float> level { 0.0f }; };
    std::array<Click, 8> clicks;
    double rate = 44100.0;
    std::atomic<int> lastHitDrum { -1 };
};
