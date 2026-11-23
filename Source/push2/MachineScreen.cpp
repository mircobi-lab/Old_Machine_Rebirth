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
                                 juce::Typeface::Ptr font)
{
    juce::Image image (juce::Image::RGB, push2::displayWidth, push2::displayHeight, true);
    juce::Graphics g (image);

    g.fillAll (juce::Colour (0xff050506));

    auto area = image.getBounds().reduced (14, 10);

    // Titolo: la prima riga se comincia per '#'
    juce::StringArray fields (lines);
    if (! fields.isEmpty() && fields[0].startsWithChar ('#'))
    {
        auto title = area.removeFromTop (46);
        g.setColour (juce::Colour (0xfff3f3f1));
        g.setFont (screenFont (font, 40.0f));
        g.drawText (fields[0].substring (1).trim().toUpperCase(), title,
                    juce::Justification::centredLeft, false);

        // A destra il gruppo dei banchi, in negativo come sul pannello
        const juce::String group = firstBank + " - " + lastBank;
        g.setFont (screenFont (font, 26.0f));
        const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (screenFont (font, 26.0f), group)) + 20;
        auto badge = title.removeFromRight (w).withSizeKeepingCentre (w, 30);
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

        g.setColour (juce::Colour (0xff9b9da2));
        g.setFont (screenFont (font, 20.0f));
        g.drawText (label, top, juce::Justification::centredLeft, false);

        g.setColour (juce::Colour (0xfff3f3f1));
        g.setFont (screenFont (font, 30.0f));
        g.drawText (value, bottom, juce::Justification::centredLeft, false);
    }

    return image;
}

} // namespace mbstudio
