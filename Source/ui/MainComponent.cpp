#include "MainComponent.h"

#include <BinaryData.h>

MainComponent::MainComponent()
{
    setAudioChannels (2, 2);

    status.setFont (MBStudioLookAndFeel::titleFont (15.0f));
    status.setColour (juce::Label::textColourId, juce::Colour (0xff9b9da2));
    status.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (status);

    audioButton.onClick = [this]
    {
        auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
            deviceManager, 1, 2, 1, 2, false, false, true, false);
        selector->setSize (500, 420);

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (selector.release());
        o.dialogTitle = "SCHEDA AUDIO";
        o.dialogBackgroundColour = juce::Colour (0xff0a0a0b);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    };
    addAndMakeVisible (audioButton);

    bankButton.onClick = [this] { mpc.setBankGroup (mpc.bankGroup() == 0 ? 1 : 0); };
    addAndMakeVisible (bankButton);

    oscButton.onClick = [this] { showOscSettings(); };
    addAndMakeVisible (oscButton);

    loadButton.onClick = [this] { loadOntoSelected(); };
    addAndMakeVisible (loadButton);

    mpc.onChanged = [this]
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safe != nullptr)
            {
                safe->refreshSurfaces();
                safe->repaint();
            }
        });
    };

    auto face = juce::Typeface::createSystemTypefaceFor (BinaryData::BebasNeueRegular_ttf,
                                                         BinaryData::BebasNeueRegular_ttfSize);
    push = std::make_unique<Push2Panel> (face);
    push->onPadDown = [this] (machine::PadRef p, int velocity) { mpc.padDown (p, velocity); };
    push->onPadUp   = [this] (machine::PadRef p)               { mpc.padUp (p); };
    push->onCommand = [this] (const control::Control& c, bool down) { mpc.command (c, down); };
    push->onEncoder = [this] (const control::Control& c, int steps) { mpc.encoder (c, steps); };
    push->onBankGroupChanged = [this] (int g) { mpc.setBankGroup (g); };

    startOsc();
    refreshSurfaces();

    setSize (1100, 700);
    startTimerHz (10);
}

MainComponent::~MainComponent()
{
    stopTimer();
    mpc.onChanged = nullptr;
    push = nullptr;
    osc = nullptr;
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int, double sampleRate)
{
    mpc.prepare (sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer == nullptr || info.numSamples <= 0)
        return;

    // una finestra sul pezzo di buffer che tocca a noi, senza copiare niente
    juce::AudioBuffer<float> view (info.buffer->getArrayOfWritePointers(),
                                   info.buffer->getNumChannels(),
                                   info.startSample, info.numSamples);
    mpc.process (view);
}

void MainComponent::releaseResources() {}

//==============================================================================
juce::File MainComponent::oscSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Old Machine Rebirth")
               .getChildFile ("osc.txt");
}

void MainComponent::startOsc()
{
    // host, porta di ascolto, porta di risposta: una riga, separati da spazio
    juce::String host = "127.0.0.1";
    int in = 9000, out = 9001;

    if (auto text = oscSettingsFile().loadFileAsString(); text.isNotEmpty())
    {
        auto parts = juce::StringArray::fromTokens (text.trim(), " ", "");
        if (parts.size() >= 1 && parts[0].isNotEmpty()) host = parts[0];
        if (parts.size() >= 2) in = parts[1].getIntValue();
        if (parts.size() >= 3) out = parts[2].getIntValue();
    }

    osc = std::make_unique<OscSurface>();
    osc->onPadDown = [this] (machine::PadRef p, int velocity) { mpc.padDown (p, velocity); };
    osc->onPadUp   = [this] (machine::PadRef p)               { mpc.padUp (p); };
    osc->onCommand = [this] (const control::Control& c, bool down) { mpc.command (c, down); };
    osc->onEncoder = [this] (const control::Control& c, int steps) { mpc.encoder (c, steps); };
    osc->start (in, host, out);

    refreshSurfaces();
}

void MainComponent::showOscSettings()
{
    auto* w = new juce::AlertWindow ("OSC", "Dove manda e dove ascolta la macchina. "
                                            "Con l'iPhone che fa da access point, metti qui l'indirizzo del telefono.",
                                     juce::MessageBoxIconType::NoIcon);
    juce::String host = "127.0.0.1", in = "9000", out = "9001";
    if (auto text = oscSettingsFile().loadFileAsString(); text.isNotEmpty())
    {
        auto parts = juce::StringArray::fromTokens (text.trim(), " ", "");
        if (parts.size() >= 1) host = parts[0];
        if (parts.size() >= 2) in = parts[1];
        if (parts.size() >= 3) out = parts[2];
    }

    w->addTextEditor ("host", host, "INDIRIZZO A CUI RISPONDERE");
    w->addTextEditor ("in", in, "PORTA DI ASCOLTO");
    w->addTextEditor ("out", out, "PORTA DI RISPOSTA");
    w->addButton ("SALVA", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("LASCIA", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
        {
            const auto line = w->getTextEditorContents ("host").trim() + " "
                            + w->getTextEditorContents ("in").trim() + " "
                            + w->getTextEditorContents ("out").trim();
            oscSettingsFile().getParentDirectory().createDirectory();
            oscSettingsFile().replaceWithText (line);
            startOsc();
        }
        delete w;
    }), false);
}

//==============================================================================
void MainComponent::refreshLights()
{
    for (int bank = 0; bank < machine::numBanks; ++bank)
        for (int pad = 1; pad <= machine::padsPerBank; ++pad)
        {
            const machine::PadRef p { bank, pad };
            const auto light = mpc.lightFor (p);

            if (push != nullptr)
                push->setPadLight (p, light);

            if (osc != nullptr)
            {
                const char* name = "empty";
                switch (light)
                {
                    case machine::PadLight::off:      name = "off"; break;
                    case machine::PadLight::empty:    name = "empty"; break;
                    case machine::PadLight::loaded:   name = "loaded"; break;
                    case machine::PadLight::selected: name = "selected"; break;
                    case machine::PadLight::playing:  name = "playing"; break;
                    case machine::PadLight::muted:    name = "muted"; break;
                }
                osc->sendPadLight (p, name);
            }
        }

    for (const auto& b : control::buttons())
    {
        const bool lit = mpc.buttonLit (b);
        const int value = b.rgb ? Push2Panel::paletteIndexFor (lit ? machine::PadLight::selected
                                                                   : machine::PadLight::loaded)
                                : (lit ? 127 : 24);
        if (push != nullptr)
            push->setButtonLight (b.number, value);
        if (osc != nullptr)
            osc->sendButtonLight (b.number, b.rgb ? (lit ? 127 : 0) : value);
    }
}

void MainComponent::refreshSurfaces()
{
    if (push != nullptr)
        push->setBankGroup (mpc.bankGroup());

    refreshLights();

    const auto lines = mpc.screenLines();
    if (push != nullptr)
        push->setScreenLines (lines);
    if (osc != nullptr)
        osc->sendScreen (lines);

    bankButton.setButtonText (mpc.bankGroup() == 0 ? "BANCHI A-D" : "BANCHI E-H");
}

void MainComponent::timerCallback()
{
    juce::String t;
    if (push != nullptr && push->isConnected())
        t << "PUSH 2 " << push->portDescription() << "    ";
    else
        t << "PUSH 2 NON COLLEGATO    ";

    if (osc != nullptr)
        t << osc->description() << "    ";

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        t << dev->getName().toUpperCase() << "  "
          << juce::String (dev->getCurrentSampleRate() / 1000.0, 1) << " KHZ";
    else
        t << "NESSUNA SCHEDA AUDIO";

    t << "    VOCI " << mpc.engine().activeVoices() << "/" << SampleEngine::numVoices;

    status.setText (t, juce::dontSendNotification);
}

//==============================================================================
void MainComponent::loadOntoSelected()
{
    const auto target = mpc.selectedPad();

    chooser = std::make_unique<juce::FileChooser> (
        "Scegli un suono per il pad " + machine::bankName (target.bank) + juce::String (target.pad),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, target] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
            return;

        if (mpc.engine().loadFile (machine::drumIndex (target), file))
            refreshSurfaces();

        repaint();
    });
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg"))
            return true;
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    // Il suono va sul pad sotto il dito; se si lascia fuori dalla griglia, su
    // quello scelto. Piu' file di seguito riempiono i pad uno dopo l'altro.
    auto target = padAt ({ x, y }).value_or (mpc.selectedPad());

    for (const auto& name : files)
    {
        const juce::File file (name);
        if (! file.existsAsFile())
            continue;

        const int index = machine::drumIndex (target);
        mpc.engine().loadFile (index, file);

        if (index + 1 >= machine::numDrums)
            break;
        target = machine::drumAt (index + 1);
    }

    refreshSurfaces();
    repaint();
}

//==============================================================================
juce::Rectangle<int> MainComponent::cellBounds (int row, int column) const
{
    const int side = juce::jmin (gridArea.getWidth(), gridArea.getHeight()) / 8;
    const auto origin = gridArea.getCentre() - juce::Point<int> (side * 4, side * 4);
    return { origin.x + column * side, origin.y + row * side, side, side };
}

std::optional<machine::PadRef> MainComponent::padAt (juce::Point<int> point) const
{
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            if (cellBounds (row, col).contains (point))
                return machine::padForCell (row, col, mpc.bankGroup());

    return std::nullopt;
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    brand.paintPrinted (g, brandArea, "REBIRTH");
    brand.paintDebossed (g, debossArea);

    // Quello che si vede sul display del Push, anche qui
    if (push != nullptr)
    {
        const auto screen = push->lastScreen();
        g.setColour (juce::Colour (0xff050506));
        g.fillRect (screenArea);
        g.setColour (juce::Colour (0xff26282b));
        g.drawRect (screenArea, 1);
        if (screen.isValid())
            g.drawImage (screen, screenArea.reduced (1).toFloat(), juce::RectanglePlacement::centred);
    }

    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
        {
            const auto p = machine::padForCell (row, col, mpc.bankGroup());
            const auto cell = cellBounds (row, col).reduced (3);
            const auto& d = mpc.engine().drum (machine::drumIndex (p));
            const auto light = mpc.lightFor (p);

            juce::Colour edge (0xff34363a);
            float thickness = 1.0f;

            switch (light)
            {
                case machine::PadLight::selected: edge = juce::Colour (0xffffb020); thickness = 1.6f; break;
                case machine::PadLight::muted:    edge = juce::Colour (0xffff3b30); break;
                case machine::PadLight::loaded:   edge = juce::Colour (0xff3d8bff); break;
                default: break;
            }

            g.setColour (juce::Colour (0xff121314));
            g.fillRoundedRectangle (cell.toFloat(), 3.0f);

            if (d.loaded())
            {
                g.setColour (edge.withAlpha (0.16f));
                g.fillRoundedRectangle (cell.toFloat(), 3.0f);
            }

            g.setColour (edge);
            g.drawRoundedRectangle (cell.toFloat(), 3.0f, thickness);

            g.setColour (juce::Colour (0xff9b9da2));
            g.setFont (MBStudioLookAndFeel::titleFont (14.0f));
            g.drawText (machine::bankName (p.bank) + juce::String (p.pad),
                        cell.reduced (5), juce::Justification::bottomLeft, false);

            if (d.loaded())
            {
                g.setColour (juce::Colour (0xfff3f3f1));
                g.setFont (MBStudioLookAndFeel::titleFont (12.0f));
                g.drawText (d.name, cell.reduced (4).removeFromTop (16),
                            juce::Justification::topLeft, false);
            }
        }

    // I quattro quadranti sono quattro banchi: linea incisa fra loro
    const auto middle = cellBounds (0, 0).getUnion (cellBounds (7, 7));
    g.setColour (juce::Colour (0xff1f2023));
    g.drawVerticalLine (middle.getCentreX(), (float) middle.getY(), (float) middle.getBottom());
    g.drawHorizontalLine (middle.getCentreY(), (float) middle.getX(), (float) middle.getRight());
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto top = r.removeFromTop (40);
    audioButton.setBounds (top.removeFromRight (110).reduced (2));
    top.removeFromRight (8);
    oscButton.setBounds (top.removeFromRight (90).reduced (2));
    top.removeFromRight (8);
    loadButton.setBounds (top.removeFromRight (110).reduced (2));
    top.removeFromRight (8);
    bankButton.setBounds (top.removeFromRight (140).reduced (2));
    brandArea = top;

    auto bottom = r.removeFromBottom (28);
    debossArea = bottom.removeFromRight (150);
    status.setBounds (bottom);

    const int screenHeight = juce::jmin (160, r.getHeight() / 4);
    screenArea = r.removeFromTop (screenHeight)
                  .withSizeKeepingCentre (juce::jmin (r.getWidth(), 960), screenHeight);
    gridArea = r.reduced (0, 10);
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    if (auto p = padAt (e.getPosition()))
    {
        mousePad = p;
        mpc.padDown (*p, 100);
    }
}

void MainComponent::mouseUp (const juce::MouseEvent&)
{
    if (mousePad.has_value())
    {
        mpc.padUp (*mousePad);
        mousePad.reset();
    }
}
