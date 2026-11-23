#include "Waveform.h"

namespace mbstudio
{

WaveView computePeaks (const juce::AudioBuffer<float>& buffer, int columns)
{
    WaveView view;

    const int length = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();
    columns = juce::jlimit (1, 4096, columns);

    if (length <= 0 || channels <= 0)
        return view;

    view.lows.resize ((size_t) columns, 0.0f);
    view.highs.resize ((size_t) columns, 0.0f);

    const double perColumn = (double) length / (double) columns;

    for (int c = 0; c < columns; ++c)
    {
        const int from = (int) (perColumn * c);
        const int to = juce::jmin (length, juce::jmax (from + 1, (int) (perColumn * (c + 1))));

        float low = 0.0f, high = 0.0f;

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (
                                   buffer.getReadPointer (ch) + from, to - from);
            low = juce::jmin (low, range.getStart());
            high = juce::jmax (high, range.getEnd());
        }

        view.lows[(size_t) c] = juce::jlimit (-1.0f, 1.0f, low);
        view.highs[(size_t) c] = juce::jlimit (-1.0f, 1.0f, high);
    }

    return view;
}

void drawWave (juce::Graphics& g, juce::Rectangle<int> area, const WaveView& view,
               juce::Typeface::Ptr font)
{
    if (area.getWidth() <= 2 || area.getHeight() <= 2)
        return;

    // la cornice incassata, come le altre zone del pannello
    g.setColour (juce::Colour (0xff050506));
    g.fillRect (area);
    g.setColour (juce::Colour (0xff26282b));
    g.drawRect (area, 1);

    auto inner = area.reduced (2);
    const float middle = (float) inner.getCentreY();
    const float half = (float) inner.getHeight() * 0.5f - 1.0f;

    // la riga dello zero
    g.setColour (juce::Colour (0xff1f2023));
    g.drawHorizontalLine ((int) middle, (float) inner.getX(), (float) inner.getRight());

    if (view.isEmpty())
        return;

    const int columns = (int) view.lows.size();
    const auto fractionToX = [&inner, columns] (double f)
    {
        juce::ignoreUnused (columns);
        return (float) inner.getX() + (float) (juce::jlimit (0.0, 1.0, f) * inner.getWidth());
    };

    // Il pezzo fuori dal ritaglio si vede smorto: cosi' si capisce che c'e'
    // ancora, ma che non suona.
    for (int x = 0; x < inner.getWidth(); ++x)
    {
        const double f = (double) x / (double) juce::jmax (1, inner.getWidth() - 1);
        const int column = juce::jlimit (0, columns - 1, (int) (f * columns));

        const bool inside = f >= view.startFraction && f <= view.endFraction;
        g.setColour (inside ? view.colour : view.colour.withAlpha (0.22f));

        const float low = view.lows[(size_t) column];
        const float high = view.highs[(size_t) column];
        const float top = middle - high * half;
        const float bottom = middle - low * half;

        g.drawVerticalLine (inner.getX() + x, juce::jmin (top, middle - 0.5f),
                            juce::jmax (bottom, middle + 0.5f));
    }

    // I tagli proposti dalle fette: righe verticali bianche smorte, numerate
    // quando c'e' spazio.
    const auto labelFont = font != nullptr
                               ? juce::Font (juce::FontOptions (font).withHeight (14.0f))
                               : juce::Font (juce::FontOptions (14.0f));

    const bool roomForNumbers = view.slices.size() <= 16;
    int number = 2;

    // la prima fetta comincia all'inizio e non ha una riga sua: il numero si
    // scrive lo stesso, altrimenti sembra che si parta dalla due
    if (roomForNumbers && ! view.slices.empty())
    {
        g.setFont (labelFont);
        g.setColour (juce::Colour (0xff9b9da2));
        g.drawText ("1", juce::Rectangle<int> (inner.getX() + 2, inner.getY(), 22, 16),
                    juce::Justification::topLeft, false);
    }

    for (auto slice : view.slices)
    {
        const float x = fractionToX (slice);
        g.setColour (juce::Colour (0xfff3f3f1).withAlpha (0.5f));
        g.drawVerticalLine ((int) x, (float) inner.getY(), (float) inner.getBottom());

        if (roomForNumbers)
        {
            g.setFont (labelFont);
            g.setColour (juce::Colour (0xff9b9da2));
            g.drawText (juce::String (number),
                        juce::Rectangle<int> ((int) x + 2, inner.getY(), 22, 16),
                        juce::Justification::topLeft, false);
        }

        ++number;
    }

    // Inizio e fine del ritaglio: due righe piene con la linguetta, come le
    // maniglie di un editor.
    const auto handle = [&g, &inner] (float x, bool pointingRight)
    {
        g.setColour (juce::Colour (0xfff3f3f1));
        g.drawVerticalLine ((int) x, (float) inner.getY(), (float) inner.getBottom());

        const float w = 6.0f;
        juce::Path tab;
        tab.startNewSubPath (x, (float) inner.getY());
        tab.lineTo (x + (pointingRight ? w : -w), (float) inner.getY());
        tab.lineTo (x, (float) inner.getY() + w);
        tab.closeSubPath();
        g.fillPath (tab);
    };

    handle (fractionToX (view.startFraction), true);
    handle (fractionToX (view.endFraction), false);

    // Dove sta suonando
    if (view.playFraction >= 0.0)
    {
        g.setColour (juce::Colour (0xff3ee05c));
        g.drawVerticalLine ((int) fractionToX (view.playFraction),
                            (float) inner.getY(), (float) inner.getBottom());
    }

    if (view.caption.isNotEmpty())
    {
        g.setFont (labelFont);
        g.setColour (juce::Colour (0xff9b9da2));
        g.drawText (view.caption.toUpperCase(), inner.reduced (4, 2),
                    juce::Justification::bottomRight, false);
    }
}

} // namespace mbstudio
