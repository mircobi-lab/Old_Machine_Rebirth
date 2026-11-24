#include "Chopper.h"

#include "Engine/SampleForgeEngine.h"

namespace chopper
{

namespace
{
juce::String bankName (const std::string& id)
{
    if (id == "drums")  return "BATTERIA";
    if (id == "vox")    return "VOCE";
    if (id == "loops")  return "GIRI";
    if (id == "chords") return "ACCORDI";
    if (id == "bass")   return "BASSO";
    return juce::String (id).toUpperCase();
}

sound::Kind kindFor (const std::string& id)
{
    if (id == "drums")  return sound::Kind::rullante;    // batteria mischiata
    if (id == "vox")    return sound::Kind::voce;
    if (id == "loops")  return sound::Kind::giro;
    if (id == "chords") return sound::Kind::accordi;
    if (id == "bass")   return sound::Kind::basso;
    return sound::Kind::generico;
}

juce::AudioBuffer<float> toBuffer (const sf::Stereo& s)
{
    const int length = (int) s.size();
    if (length <= 0)
        return {};

    const bool stereo = s.R.size() == s.L.size();
    juce::AudioBuffer<float> b (stereo ? 2 : 1, length);
    b.copyFrom (0, 0, s.L.data(), length);
    if (stereo)
        b.copyFrom (1, 0, s.R.data(), length);
    return b;
}
}

Result chop (const juce::File& file, int secondsToUse,
             std::function<void (double, juce::String)> progress)
{
    Result result;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
    {
        result.error = "NON LEGGO QUEL FILE";
        return result;
    }

    const double rate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
    const int length = (int) juce::jmin (reader->lengthInSamples,
                                         (juce::int64) (rate * (double) juce::jmax (5, secondsToUse)));

    juce::AudioBuffer<float> input ((int) juce::jmin (reader->numChannels, 2u), length);
    reader->read (&input, 0, length, 0, true, true);

    sf::Sources sources;
    sources.stems = false;
    sources.mix.L.assign (input.getReadPointer (0), input.getReadPointer (0) + length);
    if (input.getNumChannels() > 1)
        sources.mix.R.assign (input.getReadPointer (1), input.getReadPointer (1) + length);
    else
        sources.mix.R = sources.mix.L;

    sf::Progress report;
    if (progress)
        report = [progress] (double fraction, const std::string& message)
        {
            progress (fraction, juce::String (message));
        };

    try
    {
        sf::Engine engine;
        const auto analysed = engine.analyze (std::move (sources), rate, (double) secondsToUse, report);

        result.bpm = analysed.bpm;
        result.key = juce::String (analysed.key.name);

        for (const auto& bank : analysed.banks)
            for (const auto& pad : bank.pads)
            {
                if (pad == nullptr || pad->audio.size() == 0)
                    continue;

                Slice slice;
                slice.bank = bankName (bank.id);
                slice.kind = kindFor (bank.id);
                slice.name = juce::String (pad->label).toUpperCase();
                if (slice.name.isEmpty())
                    slice.name = slice.bank;
                slice.audio = toBuffer (pad->audio);
                slice.sampleRate = analysed.sr > 0 ? analysed.sr : rate;

                if (slice.audio.getNumSamples() > 0)
                    result.slices.push_back (std::move (slice));
            }

        if (result.slices.empty())
            result.error = "NON HO TROVATO FETTE";
    }
    catch (const std::exception& e)
    {
        result.error = juce::String (e.what()).toUpperCase();
    }
    catch (...)
    {
        result.error = "L'ANALISI SI E' FERMATA";
    }

    return result;
}

} // namespace chopper
