#include "ui/MainComponent.h"
#include "ui/MBStudioLookAndFeel.h"

#include <juce_gui_extra/juce_gui_extra.h>

// Old Machine Rebirth: una macchina da campionamento col Push 2 come pannello e
// il Mac che fa i conti.
class RebirthApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Old Machine Rebirth"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        window = std::make_unique<Window>();
    }

    void shutdown() override
    {
        window = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    class Window final : public juce::DocumentWindow
    {
    public:
        Window() : juce::DocumentWindow ("OLD MACHINE REBIRTH",
                                         juce::Colours::black,
                                         juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, false);
            centreWithSize (1100, 700);
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    MBStudioLookAndFeel lookAndFeel;
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (RebirthApplication)
