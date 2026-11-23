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

    bankButton.onClick = [this]
    {
        if (push != nullptr)
            push->setBankGroup (push->getBankGroup() == 0 ? 1 : 0);
        repaint();
    };
    addAndMakeVisible (bankButton);

    oscButton.onClick = [this] { showOscSettings(); };
    addAndMakeVisible (oscButton);

    auto face = juce::Typeface::createSystemTypefaceFor (BinaryData::BebasNeueRegular_ttf,
                                                         BinaryData::BebasNeueRegular_ttfSize);
    push = std::make_unique<Push2Panel> (face);
    push->onPadDown = [this] (machine::PadRef p, int velocity)
    {
        hit (p, velocity);
        juce::MessageManager::callAsync ([this] { repaint(); });
    };
    push->onPadUp = [this] (machine::PadRef p)
    {
        padLight (p, machine::drumIndex (p) == machine::drumIndex (selected)
                         ? Push2Panel::PadLight::selected : Push2Panel::PadLight::empty);
    };

    push->onCommand = [this] (const control::Control& c, bool down)
    {
        command (c, down);
        juce::MessageManager::callAsync ([this] { repaint(); });
    };
    push->onEncoder = [this] (const control::Control& c, int delta)
    {
        lastCommand = c.label + " " + (delta > 0 ? "+" : "") + juce::String (delta);
        updateScreen();
    };
    engine.loadStarterKit();

    loadButton.onClick = [this] { loadOntoSelected(); };
    addAndMakeVisible (loadButton);

    startOsc();
    refreshButtonLights();

    setSize (1100, 700);
    startTimerHz (10);
    updateScreen();
}

MainComponent::~MainComponent()
{
    stopTimer();
    push = nullptr;
    shutdownAudio();
}

void MainComponent::prepareToPlay (int, double sampleRate)
{
    engine.prepare (sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer == nullptr || info.numSamples <= 0)
        return;

    // una finestra sul pezzo di buffer che tocca a noi, senza copiare niente
    juce::AudioBuffer<float> view (info.buffer->getArrayOfWritePointers(),
                                   info.buffer->getNumChannels(),
                                   info.startSample, info.numSamples);
    engine.process (view);
}

void MainComponent::releaseResources() {}

void MainComponent::hit (machine::PadRef p, int velocity)
{
    const int index = machine::drumIndex (p);
    selected = p;
    lastHitDrum.store (index);
    padLight (p, Push2Panel::PadLight::playing);
    engine.trigger (index, juce::jlimit (0.0f, 1.0f, (float) velocity / 127.0f));
    updateScreen();
}

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
    osc->onPadDown = [this] (machine::PadRef p, int velocity)
    {
        hit (p, velocity);
        repaint();
    };
    osc->onPadUp = [this] (machine::PadRef p)
    {
        padLight (p, lightFor (p));
    };
    osc->onCommand = [this] (const control::Control& c, bool down) { command (c, down); repaint(); };
    osc->onEncoder = [this] (const control::Control& c, int delta)
    {
        lastCommand = c.label + " " + (delta > 0 ? "+" : "") + juce::String (delta);
        updateScreen();
    };
    osc->start (in, host, out);

    refreshButtonLights();
    updateScreen();
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
    w->addButton ("SALVA", 1);
    w->addButton ("LASCIA", 0);

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

Push2Panel::PadLight MainComponent::lightFor (machine::PadRef p) const
{
    if (machine::drumIndex (p) == machine::drumIndex (selected))
        return Push2Panel::PadLight::selected;
    return engine.drum (machine::drumIndex (p)).loaded() ? Push2Panel::PadLight::loaded
                                                         : Push2Panel::PadLight::empty;
}

std::optional<machine::PadRef> MainComponent::padAt (juce::Point<int> point) const
{
    const int group = push != nullptr ? push->getBankGroup() : 0;

    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            if (cellBounds (row, col).contains (point))
                return machine::padForCell (row, col, group);

    return std::nullopt;
}

void MainComponent::loadOntoSelected()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Scegli un suono per il pad " + machine::bankName (selected.bank) + juce::String (selected.pad),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
            return;

        const int index = machine::drumIndex (selected);
        if (engine.loadFile (index, file))
        {
            lastCommand = "CARICATO " + engine.drum (index).name;
            padLight (selected, lightFor (selected));
            updateScreen();
            repaint();
        }
        else
            lastCommand = "NON LEGGO QUEL FILE";
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
    auto target = padAt ({ x, y }).value_or (selected);

    for (const auto& name : files)
    {
        const juce::File file (name);
        if (! file.existsAsFile())
            continue;

        const int index = machine::drumIndex (target);
        if (engine.loadFile (index, file))
        {
            selected = target;
            lastCommand = "CARICATO " + engine.drum (index).name;
            padLight (target, lightFor (target));
        }

        if (index + 1 >= machine::numDrums)
            break;
        target = machine::drumAt (index + 1);
    }

    updateScreen();
    repaint();
}

void MainComponent::padLight (machine::PadRef p, Push2Panel::PadLight light)
{
    if (push != nullptr)
        push->setPadLight (p, light);

    if (osc != nullptr)
    {
        const char* name = "empty";
        switch (light)
        {
            case Push2Panel::PadLight::off:      name = "off"; break;
            case Push2Panel::PadLight::empty:    name = "empty"; break;
            case Push2Panel::PadLight::loaded:   name = "loaded"; break;
            case Push2Panel::PadLight::selected: name = "selected"; break;
            case Push2Panel::PadLight::playing:  name = "playing"; break;
        }
        osc->sendPadLight (p, name);
    }
}

void MainComponent::buttonLight (int cc, int value, bool rgb)
{
    if (push != nullptr)
        push->setButtonLight (cc, value);
    if (osc != nullptr)
        osc->sendButtonLight (cc, rgb ? (value > 1 ? 127 : 0) : value);
}

void MainComponent::command (const control::Control& c, bool down)
{
    if (! down)
        return;

    lastCommand = c.label;

    using C = control::Command;
    switch (c.command)
    {
        case C::pageSample: case C::pageChop: case C::pageSound: case C::pageSequence:
        case C::pageStep: case C::pageSong: case C::pageMix: case C::pageDisk:
            page = c.command;
            pageName = c.label;
            break;

        case C::quantiseValue:
            quantise = c.index;
            break;

        case C::layout:
            push->setBankGroup (push->getBankGroup() == 0 ? 1 : 0);
            break;

        default:
            break;   // il resto arriva col motore
    }

    refreshButtonLights();
    updateScreen();
}

void MainComponent::refreshButtonLights()
{

    using C = control::Command;
    for (const auto& b : control::buttons())
    {
        const bool lit = (b.command == page)
                      || (b.command == C::quantiseValue && b.index == quantise);
        if (b.rgb)
            buttonLight (b.number, Push2Panel::paletteIndexFor (lit ? Push2Panel::PadLight::selected
                                                                    : Push2Panel::PadLight::loaded), true);
        else
            buttonLight (b.number, lit ? 127 : 24, false);
    }
}

void MainComponent::updateScreen()
{
    const int drum = lastHitDrum.load();
    juce::StringArray lines;
    juce::ignoreUnused (drum);
    const auto& d = engine.drum (machine::drumIndex (selected));

    lines.add ("#" + pageName);
    lines.add ("PAD|" + machine::bankName (selected.bank) + juce::String (selected.pad));
    lines.add ("SUONO|" + (d.loaded() ? d.name : juce::String ("VUOTO")));
    lines.add ("QUANTIZZA|" + control::quantiseName (quantise));
    lines.add ("LIVELLO|" + juce::String (juce::roundToInt (d.level * 100.0f)));
    lines.add ("ACCORDA|" + juce::String (d.tuneSemitones > 0 ? "+" : "") + juce::String (d.tuneSemitones));
    lines.add ("VOCI|" + juce::String (engine.activeVoices()) + "/" + juce::String (SampleEngine::numVoices));
    lines.add ("SUONI|" + juce::String (engine.loadedCount()));
    lines.add ("TASTO|" + (lastCommand.isEmpty() ? juce::String ("-") : lastCommand));
    if (push != nullptr)
        push->setScreenLines (lines);
    if (osc != nullptr)
        osc->sendScreen (lines);
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
        t << dev->getName().toUpperCase() << "  " << juce::String (dev->getCurrentSampleRate() / 1000.0, 1) << " KHZ";
    else
        t << "NESSUNA SCHEDA AUDIO";

    status.setText (t, juce::dontSendNotification);

    if (push != nullptr)
        bankButton.setButtonText (push->getBankGroup() == 0 ? "BANCHI A-D" : "BANCHI E-H");
}

juce::Rectangle<int> MainComponent::cellBounds (int row, int column) const
{
    const int side = juce::jmin (gridArea.getWidth(), gridArea.getHeight()) / 8;
    const auto origin = gridArea.getCentre() - juce::Point<int> (side * 4, side * 4);
    return { origin.x + column * side, origin.y + row * side, side, side };
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
            g.drawImage (screen, screenArea.reduced (1).toFloat(),
                         juce::RectanglePlacement::centred);
    }

    const int group = push != nullptr ? push->getBankGroup() : 0;

    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
        {
            const auto p = machine::padForCell (row, col, group);
            const auto cell = cellBounds (row, col).reduced (3);
            const bool isSelected = machine::drumIndex (p) == machine::drumIndex (selected);

            const auto& d = engine.drum (machine::drumIndex (p));

            g.setColour (juce::Colour (0xff121314));
            g.fillRoundedRectangle (cell.toFloat(), 3.0f);

            if (d.loaded())
            {
                g.setColour (juce::Colour (0xff3d8bff).withAlpha (0.18f));
                g.fillRoundedRectangle (cell.toFloat(), 3.0f);
            }

            g.setColour (juce::Colour (isSelected ? 0xffffb020 : (d.loaded() ? 0xff3d8bff : 0xff34363a)));
            g.drawRoundedRectangle (cell.toFloat(), 3.0f, isSelected ? 1.6f : 1.0f);

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

    screenArea = r.removeFromTop (juce::jmin (160, r.getHeight() / 4))
                   .withSizeKeepingCentre (juce::jmin (r.getWidth(), 960), juce::jmin (160, r.getHeight() / 4));
    gridArea = r.reduced (0, 10);
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    const int group = push != nullptr ? push->getBankGroup() : 0;

    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            if (cellBounds (row, col).contains (e.getPosition()))
            {
                hit (machine::padForCell (row, col, group), 100);
                repaint();
                return;
            }
}
