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
        push->setPadLight (p, machine::drumIndex (p) == machine::drumIndex (selected)
                                  ? Push2Panel::PadLight::selected : Push2Panel::PadLight::empty);
    };

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
    rate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    for (auto& c : clicks)
    {
        int remaining = c.remaining.load();
        if (remaining <= 0)
            continue;

        const float level = c.level.load();
        const int n = juce::jmin (remaining, info.numSamples);

        for (int i = 0; i < n; ++i)
        {
            const float env = (float) (remaining - i) / (float) (0.05 * rate);
            const float s = level * env * env * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                                  * 220.0 * (double) i / rate);
            for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
                info.buffer->addSample (ch, info.startSample + i, s);
        }
        c.remaining.store (remaining - n);
    }
}

void MainComponent::releaseResources() {}

void MainComponent::hit (machine::PadRef p, int velocity)
{
    selected = p;
    lastHitDrum.store (machine::drumIndex (p));
    push->setPadLight (p, Push2Panel::PadLight::playing);

    for (auto& c : clicks)
        if (c.remaining.load() <= 0)
        {
            c.level.store (juce::jlimit (0.0f, 1.0f, (float) velocity / 127.0f) * 0.4f);
            c.remaining.store ((int) (0.05 * rate));
            break;
        }

    updateScreen();
}

void MainComponent::updateScreen()
{
    if (push == nullptr)
        return;

    const int drum = lastHitDrum.load();
    juce::StringArray lines;
    lines.add ("#OLD MACHINE REBIRTH");
    lines.add ("PAD|" + machine::bankName (selected.bank) + juce::String (selected.pad));
    lines.add ("SUONO|" + juce::String (drum < 0 ? juce::String ("-") : juce::String (drum + 1)));
    lines.add ("LIVELLO|--");
    lines.add ("ACCORDA|--");
    lines.add ("INIZIO|--");
    lines.add ("FINE|--");
    lines.add ("DISSOLV|--");
    lines.add ("USCITA|1-2");
    push->setScreenLines (lines);
}

void MainComponent::timerCallback()
{
    juce::String t;
    if (push != nullptr && push->isConnected())
        t << "PUSH 2 " << push->portDescription() << "    ";
    else
        t << "PUSH 2 NON COLLEGATO    ";

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

    const int group = push != nullptr ? push->getBankGroup() : 0;

    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
        {
            const auto p = machine::padForCell (row, col, group);
            const auto cell = cellBounds (row, col).reduced (3);
            const bool isSelected = machine::drumIndex (p) == machine::drumIndex (selected);

            g.setColour (juce::Colour (0xff121314));
            g.fillRoundedRectangle (cell.toFloat(), 3.0f);
            g.setColour (juce::Colour (isSelected ? 0xffffb020 : 0xff34363a));
            g.drawRoundedRectangle (cell.toFloat(), 3.0f, isSelected ? 1.6f : 1.0f);

            g.setColour (juce::Colour (0xff9b9da2));
            g.setFont (MBStudioLookAndFeel::titleFont (14.0f));
            g.drawText (machine::bankName (p.bank) + juce::String (p.pad),
                        cell.reduced (5), juce::Justification::bottomLeft, false);
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
    bankButton.setBounds (top.removeFromRight (140).reduced (2));
    brandArea = top;

    auto bottom = r.removeFromBottom (28);
    debossArea = bottom.removeFromRight (150);
    status.setBounds (bottom);

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
