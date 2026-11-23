#include "Library.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace library
{

namespace
{
juce::String safeName (const juce::String& name)
{
    auto clean = name.retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz0123456789 -_");
    clean = clean.trim();
    return clean.isEmpty() ? juce::String ("SENZA NOME") : clean;
}

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double rate)
{
    if (buffer.getNumSamples() <= 0)
        return false;

    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), rate > 0 ? rate : 44100.0,
                             (unsigned int) juce::jlimit (1, 2, buffer.getNumChannels()),
                             24, {}, 0));
    if (writer == nullptr)
        return false;

    stream.release();
    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

juce::String padFileName (int index, const juce::String& name)
{
    return juce::String (index + 1).paddedLeft ('0', 3) + " " + safeName (name) + ".wav";
}
}

juce::File root()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("Old Machine Rebirth");
}

juce::File scenesFolder()
{
    auto folder = root().getChildFile ("Scene");
    folder.createDirectory();
    return folder;
}

juce::File samplesFolder()
{
    auto folder = root().getChildFile ("Libreria");
    folder.createDirectory();
    return folder;
}

juce::StringArray sceneNames()
{
    juce::StringArray names;
    for (const auto& entry : juce::RangedDirectoryIterator (scenesFolder(), false, "*",
                                                            juce::File::findDirectories))
        if (entry.getFile().getChildFile ("scena.txt").existsAsFile())
            names.add (entry.getFile().getFileName());

    names.sort (true);
    return names;
}

juce::File sceneFolder (const juce::String& name)
{
    return scenesFolder().getChildFile (safeName (name));
}

bool saveScene (const juce::File& folder, const machine::Machine& m, juce::String& error)
{
    error.clear();

    if (! folder.createDirectory())
    {
        error = "NON CI RIESCO";
        return false;
    }

    // i wav di prima vanno via, altrimenti restano in giro i pad cancellati
    for (const auto& old : juce::RangedDirectoryIterator (folder, false, "*.wav"))
        old.getFile().deleteFile();

    const auto state = m.snapshot();
    const auto& engine = m.engine();

    juce::StringArray lines;
    lines.add ("# Old Machine Rebirth - scena");
    lines.add ("tempo " + juce::String (state.bpm, 2));
    lines.add ("passo " + juce::String (state.quantise));
    lines.add ("banchi " + juce::String (state.bankGroup));
    lines.add ("pad " + juce::String (state.selectedPad));
    lines.add ("generale " + juce::String (state.masterGain, 3));
    lines.add ("fette " + juce::String (state.slices));

    for (int i = 0; i < machine::numDrums; ++i)
    {
        const auto& d = engine.drum (i);
        if (! d.loaded())
            continue;

        const auto file = folder.getChildFile (padFileName (i, d.name));
        if (! writeWav (file, d.sample, d.sourceRate))
        {
            error = "NON CI RIESCO";
            return false;
        }

        // una riga per pad: i numeri separati da spazio, il nome del file in
        // fondo perche' puo' contenere spazi
        juce::StringArray parts;
        parts.add ("suono");
        parts.add (juce::String (i));
        parts.add (juce::String (d.level, 3));
        parts.add (juce::String (d.tuneSemitones));
        parts.add (juce::String (d.pan, 3));
        parts.add (juce::String (d.start));
        parts.add (juce::String (d.end));
        parts.add (juce::String (d.reverse ? 1 : 0));
        parts.add (juce::String (d.muteGroup));
        parts.add (juce::String (state.mutes[(size_t) i] ? 1 : 0));
        parts.add (file.getFileName());
        lines.add (parts.joinIntoString (" "));
    }

    return folder.getChildFile ("scena.txt").replaceWithText (lines.joinIntoString ("\n") + "\n");
}

bool loadScene (const juce::File& folder, machine::Machine& m, juce::String& error)
{
    error.clear();

    const auto description = folder.getChildFile ("scena.txt");
    if (! description.existsAsFile())
    {
        error = "NON LEGGO QUEL FILE";
        return false;
    }

    juce::StringArray lines;
    lines.addLines (description.loadFileAsString());

    machine::Machine::State state;
    struct Loaded { int index; juce::File file; Drum drum; bool muted; };
    std::vector<Loaded> loaded;

    for (const auto& line : lines)
    {
        if (line.startsWithChar ('#') || line.trim().isEmpty())
            continue;

        auto parts = juce::StringArray::fromTokens (line.trim(), " ", "");
        const auto key = parts[0];

        if (key == "tempo")          state.bpm = parts[1].getDoubleValue();
        else if (key == "passo")     state.quantise = parts[1].getIntValue();
        else if (key == "banchi")    state.bankGroup = parts[1].getIntValue();
        else if (key == "pad")       state.selectedPad = parts[1].getIntValue();
        else if (key == "generale")  state.masterGain = (float) parts[1].getDoubleValue();
        else if (key == "fette")     state.slices = parts[1].getIntValue();
        else if (key == "suono" && parts.size() >= 11)
        {
            Loaded item;
            item.index = juce::jlimit (0, machine::numDrums - 1, parts[1].getIntValue());
            item.drum.level = (float) parts[2].getDoubleValue();
            item.drum.tuneSemitones = parts[3].getIntValue();
            item.drum.pan = (float) parts[4].getDoubleValue();
            item.drum.start = parts[5].getIntValue();
            item.drum.end = parts[6].getIntValue();
            item.drum.reverse = parts[7].getIntValue() != 0;
            item.drum.muteGroup = parts[8].getIntValue();
            item.muted = parts[9].getIntValue() != 0;

            // il nome del file e' tutto quello che resta, spazi compresi
            juce::StringArray rest;
            for (int i = 10; i < parts.size(); ++i)
                rest.add (parts[i]);

            item.file = folder.getChildFile (rest.joinIntoString (" "));
            loaded.push_back (std::move (item));
        }
    }

    m.clearAllPads();

    for (const auto& item : loaded)
    {
        if (! m.engine().loadFile (item.index, item.file))
            continue;

        auto& d = m.engine().drum (item.index);
        const auto name = d.name;
        const auto rate = d.sourceRate;
        auto sample = std::move (d.sample);

        d = item.drum;
        d.sample = std::move (sample);
        d.sourceRate = rate;

        // il nome scritto nel file e' quello buono, senza il numero davanti
        d.name = name.fromFirstOccurrenceOf (" ", false, false).toUpperCase();
        if (d.name.isEmpty())
            d.name = name;

        state.mutes[(size_t) item.index] = item.muted;
    }

    m.restore (state);
    return true;
}

juce::File addToLibrary (const juce::File& folder, const juce::String& name,
                         const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (buffer.getNumSamples() <= 0 || ! folder.createDirectory())
        return {};

    auto file = folder.getChildFile (safeName (name) + ".wav");

    // se c'e' gia' un campione con quel nome si aggiunge un numero
    int counter = 2;
    while (file.existsAsFile())
        file = folder.getChildFile (safeName (name) + " " + juce::String (counter++) + ".wav");

    return writeWav (file, buffer, sampleRate) ? file : juce::File();
}

int librarySize (const juce::File& folder)
{
    if (! folder.isDirectory())
        return 0;

    int count = 0;
    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.wav"))
    {
        juce::ignoreUnused (entry);
        ++count;
    }

    return count;
}

} // namespace library
