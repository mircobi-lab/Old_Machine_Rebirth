#include "Slicer.h"

namespace slicer
{

const std::vector<int>& counts()
{
    static const std::vector<int> list { 4, 8, 16, 32, 64, 128 };
    return list;
}

int nextCount (int current, int direction)
{
    const auto& list = counts();

    int at = 0;
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i] == current)
            at = i;

    at = juce::jlimit (0, (int) list.size() - 1, at + (direction >= 0 ? 1 : -1));
    return list[(size_t) at];
}

namespace
{
// Il passaggio per lo zero piu' vicino, cercato in una finestrella: taglia
// dove il suono vale zero e il clic non c'e'.
int snapToZero (const juce::AudioBuffer<float>& buffer, int at, int window)
{
    const int length = buffer.getNumSamples();
    if (at <= 0 || at >= length - 1)
        return juce::jlimit (0, length, at);

    const auto* data = buffer.getReadPointer (0);

    for (int d = 0; d < window; ++d)
    {
        const int before = at - d;
        if (before > 0 && data[before - 1] * data[before] <= 0.0f)
            return before;

        const int after = at + d;
        if (after < length - 1 && data[after] * data[after + 1] <= 0.0f)
            return after;
    }

    return at;
}
}

std::vector<chopper::Slice> cut (const juce::AudioBuffer<float>& source, double sampleRate,
                                 int pieces, const juce::String& baseName,
                                 const juce::String& bank, sound::Kind kind)
{
    std::vector<chopper::Slice> slices;

    const int length = source.getNumSamples();
    const int channels = source.getNumChannels();
    pieces = juce::jlimit (1, 128, pieces);

    if (length <= 0 || channels <= 0)
        return slices;

    const double rate = sampleRate > 0 ? sampleRate : 44100.0;
    const int fade = juce::jmax (1, (int) (rate * 0.001));          // un millisecondo
    const int window = juce::jmin (256, length / juce::jmax (1, pieces * 4));
    const double step = (double) length / (double) pieces;

    // meno di cinque millesimi per fetta non e' piu' un suono
    if (step < rate * 0.005)
        return slices;

    int previous = 0;

    for (int i = 0; i < pieces; ++i)
    {
        const int nominalEnd = (int) std::llround (step * (i + 1));
        const int end = i == pieces - 1 ? length
                                        : snapToZero (source, nominalEnd, juce::jmax (1, window));
        const int start = previous;
        previous = end;

        const int size = end - start;
        if (size <= fade * 2)
            continue;

        chopper::Slice slice;
        slice.bank = bank;
        slice.kind = kind;
        slice.name = baseName + " " + juce::String (i + 1);
        slice.sampleRate = rate;
        slice.audio.setSize (channels, size);

        for (int ch = 0; ch < channels; ++ch)
            slice.audio.copyFrom (ch, 0, source, ch, start, size);

        slice.audio.applyGainRamp (0, fade, 0.0f, 1.0f);
        slice.audio.applyGainRamp (size - fade, fade, 1.0f, 0.0f);

        slices.push_back (std::move (slice));
    }

    return slices;
}

} // namespace slicer
