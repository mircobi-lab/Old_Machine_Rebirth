#include "MpcProgram.h"

namespace mpc
{

namespace
{
// Il primo figlio con uno di questi nomi, qualunque sia la versione del file.
juce::String textOf (const juce::XmlElement& parent, const juce::StringArray& names)
{
    for (const auto& name : names)
        if (auto* child = parent.getChildByName (name))
        {
            const auto text = child->getAllSubText().trim();
            if (text.isNotEmpty())
                return text;
        }

    return {};
}

double numberOf (const juce::XmlElement& parent, const juce::StringArray& names, double fallback)
{
    const auto text = textOf (parent, names);
    return text.isEmpty() ? fallback : text.getDoubleValue();
}

// Tutti gli strati di uno strumento, a qualunque profondita' stiano.
void collectLayers (const juce::XmlElement& element, juce::Array<const juce::XmlElement*>& found)
{
    for (auto* child : element.getChildIterator())
    {
        if (child->hasTagName ("Layer"))
            found.add (child);
        else
            collectLayers (*child, found);
    }
}

void collectInstruments (const juce::XmlElement& element, juce::Array<const juce::XmlElement*>& found)
{
    for (auto* child : element.getChildIterator())
    {
        if (child->hasTagName ("Instrument"))
            found.add (child);
        else
            collectInstruments (*child, found);
    }
}

PlayMode playModeFrom (const juce::String& text)
{
    const auto t = text.toUpperCase();

    if (t.contains ("NOTE OFF") || t.contains ("NOTEOFF") || t.contains ("GATE"))
        return PlayMode::tenuto;
    if (t.contains ("LOOP"))
        return PlayMode::giro;
    if (t.contains ("TOGGLE"))
        return PlayMode::avviaFerma;

    return PlayMode::unaVolta;
}
}

juce::File findSample (const juce::File& programFolder, const juce::String& name)
{
    if (name.isEmpty())
        return {};

    const juce::StringArray extensions { ".wav", ".WAV", ".aif", ".aiff", ".AIF", ".AIFF",
                                         ".flac", ".FLAC" };

    // prima li' accanto, poi nella cartella dei campioni, poi un piano sopra
    juce::Array<juce::File> folders;
    folders.add (programFolder);
    folders.add (programFolder.getChildFile ("Samples"));
    folders.add (programFolder.getChildFile ("samples"));
    folders.add (programFolder.getParentDirectory().getChildFile ("Samples"));
    folders.add (programFolder.getParentDirectory());

    for (const auto& folder : folders)
    {
        if (! folder.isDirectory())
            continue;

        for (const auto& extension : extensions)
        {
            const auto file = folder.getChildFile (name + extension);
            if (file.existsAsFile())
                return file;
        }
    }

    // ultimo tentativo: un file che si chiama cosi' da qualche parte sotto la
    // cartella del programma, perche' certi pacchetti li annidano
    for (const auto& entry : juce::RangedDirectoryIterator (programFolder, true, "*.wav;*.WAV;*.aif;*.aiff"))
        if (entry.getFile().getFileNameWithoutExtension().equalsIgnoreCase (name))
            return entry.getFile();

    return {};
}

Result loadProgram (const juce::File& xpm, machine::Machine& m)
{
    Result result;

    if (! xpm.existsAsFile())
    {
        result.error = "NON LEGGO QUEL FILE";
        return result;
    }

    auto xml = juce::XmlDocument::parse (xpm);
    if (xml == nullptr)
    {
        result.error = "NON LEGGO QUEL FILE";
        return result;
    }

    result.programName = textOf (*xml, { "ProgramName", "Name" });
    if (result.programName.isEmpty())
        result.programName = xpm.getFileNameWithoutExtension();

    juce::Array<const juce::XmlElement*> instruments;
    collectInstruments (*xml, instruments);

    if (instruments.isEmpty())
    {
        result.error = "NON E' UN PROGRAMMA";
        return result;
    }

    const auto folder = xpm.getParentDirectory();

    for (int i = 0; i < instruments.size() && i < machine::numDrums; ++i)
    {
        const auto& instrument = *instruments[i];

        // lo strumento numero N e' il pad numero N; se il file dice il suo
        // numero si crede a quello
        int pad = instrument.getIntAttribute ("number", i + 1) - 1;
        if (! juce::isPositiveAndBelow (pad, machine::numDrums))
            pad = i;

        juce::Array<const juce::XmlElement*> layers;
        collectLayers (instrument, layers);

        juce::String sampleName;
        const juce::XmlElement* layer = nullptr;

        for (auto* candidate : layers)
        {
            const auto name = textOf (*candidate, { "SampleName", "SampleFile", "Sample" });
            if (name.isNotEmpty())
            {
                sampleName = name;
                layer = candidate;
                break;      // il primo strato pieno: gli altri sono sovrapposizioni
            }
        }

        if (sampleName.isEmpty())
            continue;

        const auto file = findSample (folder, sampleName);
        if (file == juce::File())
        {
            result.missing.add (sampleName);
            continue;
        }

        if (! m.engine().loadFile (pad, file))
        {
            result.missing.add (sampleName);
            continue;
        }

        auto& drum = m.engine().drum (pad);
        drum.name = sampleName.toUpperCase();
        drum.kind = sound::guessFromName (drum.name);

        // i parametri, quelli che si trovano: i nomi cambiano da una versione
        // all'altra, e quello che non c'e' resta come sta
        const double volume = numberOf (instrument, { "Volume" }, -1.0);
        if (volume >= 0.0 && volume <= 1.0)
            drum.level = (float) volume;

        const double pan = numberOf (instrument, { "Pan" }, -1.0);
        if (pan >= 0.0 && pan <= 1.0)
            drum.pan = (float) (pan * 2.0 - 1.0);       // 0.5 e' il centro

        drum.tuneSemitones = (int) numberOf (instrument, { "TuneCoarse", "CoarseTune", "Tune" },
                                             drum.tuneSemitones);
        drum.muteGroup = (int) numberOf (instrument, { "MuteGroup" }, drum.muteGroup);

        const auto mode = textOf (instrument, { "TriggerMode", "PlayMode", "OneShot" });
        if (mode.isNotEmpty())
            drum.playMode = playModeFrom (mode);

        if (layer != nullptr)
        {
            const int start = (int) numberOf (*layer, { "SampleStart", "SliceStart" }, 0.0);
            const int end = (int) numberOf (*layer, { "SampleEnd", "SliceEnd" }, 0.0);
            const int length = drum.sample.getNumSamples();

            if (start > 0 && start < length)
                drum.start = start;
            if (end > start && end < length)
                drum.end = end;

            const auto loop = textOf (*layer, { "SampleLoop", "Loop" });
            if (loop.equalsIgnoreCase ("true") || loop == "1")
                drum.playMode = PlayMode::giro;
        }

        ++result.padsLoaded;
    }

    if (result.padsLoaded == 0 && result.error.isEmpty())
        result.error = "NESSUN CAMPIONE TROVATO";

    return result;
}

} // namespace mpc
