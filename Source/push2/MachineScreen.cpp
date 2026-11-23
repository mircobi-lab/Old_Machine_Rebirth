#include "MachineScreen.h"

#include "Push2Protocol.h"

namespace mbstudio
{

static juce::Font screenFont (juce::Typeface::Ptr face, float height)
{
    if (face != nullptr)
        return juce::Font (juce::FontOptions (face).withHeight (height));
    return juce::Font (juce::FontOptions (height));
}

juce::Image renderMachineScreen (const juce::StringArray& lines,
                                 const juce::String& firstBank,
                                 const juce::String& lastBank,
                                 juce::Typeface::Ptr font,
                                 const WaveView& wave)
{
    juce::Image image (juce::Image::RGB, push2::displayWidth, push2::displayHeight, true);
    juce::Graphics g (image);

    g.fillAll (juce::Colour (0xff050506));

    auto area = image.getBounds().reduced (14, 10);

    // Titolo: la prima riga se comincia per '#'
    // Con l'onda in mezzo il titolo si stringe e i campi pure: la fascia
    // dell'onda vale piu' di qualche pixel di scritta.
    const bool withWave = ! wave.isEmpty();
    const int titleHeight = withWave ? 34 : 46;
    const int titleFontHeight = withWave ? 30 : 40;

    juce::StringArray fields (lines);
    if (! fields.isEmpty() && fields[0].startsWithChar ('#'))
    {
        auto title = area.removeFromTop (titleHeight);
        g.setColour (juce::Colour (0xfff3f3f1));
        g.setFont (screenFont (font, (float) titleFontHeight));
        g.drawText (fields[0].substring (1).trim().toUpperCase(), title,
                    juce::Justification::centredLeft, false);

        // A destra il gruppo dei banchi, in negativo come sul pannello
        const juce::String group = firstBank + " - " + lastBank;
        const float badgeFont = withWave ? 22.0f : 26.0f;
        g.setFont (screenFont (font, badgeFont));
        const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (screenFont (font, badgeFont), group)) + 20;
        auto badge = title.removeFromRight (w).withSizeKeepingCentre (w, withWave ? 26 : 30);
        g.setColour (juce::Colour (0xfff3f3f1));
        g.fillRect (badge);
        g.setColour (juce::Colour (0xff0b0b0c));
        g.drawText (group, badge, juce::Justification::centred, false);

        fields.remove (0);
    }

    // Riga incisa fra il titolo e i campi
    const int lineY = area.getY() + 4;
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (lineY, (float) area.getX(), (float) area.getRight());
    g.setColour (juce::Colour (0xff1f2023));
    g.drawHorizontalLine (lineY + 1, (float) area.getX(), (float) area.getRight());
    area.removeFromTop (12);

    // L'onda, quando c'e': prende quello che resta sopra i campi
    if (withWave)
    {
        const int fieldsHeight = 42;
        auto waveArea = area.removeFromTop (juce::jmax (0, area.getHeight() - fieldsHeight - 4));
        drawWave (g, waveArea, wave, font);
        area.removeFromTop (4);
    }

    // Gli otto campi, uno per rotativo
    const int columns = 8;
    const int columnWidth = area.getWidth() / columns;

    for (int i = 0; i < juce::jmin (columns, fields.size()); ++i)
    {
        const auto cell = juce::Rectangle<int> (area.getX() + i * columnWidth, area.getY(),
                                                columnWidth, area.getHeight()).reduced (6, 0);
        const auto label = fields[i].upToFirstOccurrenceOf ("|", false, false).trim().toUpperCase();
        const auto value = fields[i].fromFirstOccurrenceOf ("|", false, false).trim().toUpperCase();

        auto top = cell;
        auto bottom = top.removeFromBottom (cell.getHeight() / 2);

        // le scritte si restringono se non ci stanno: meglio una parola
        // piccola che una parola tagliata a meta'
        g.setColour (juce::Colour (0xff9b9da2));
        g.setFont (screenFont (font, withWave ? 17.0f : 20.0f));
        g.drawFittedText (label, top, juce::Justification::centredLeft, 1, 0.7f);

        g.setColour (juce::Colour (0xfff3f3f1));
        g.setFont (screenFont (font, withWave ? 25.0f : 30.0f));
        g.drawFittedText (value, bottom, juce::Justification::centredLeft, 1, 0.6f);
    }

    return image;
}

} // namespace mbstudio
