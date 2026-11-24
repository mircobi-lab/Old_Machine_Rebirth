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

// Mette il campione sul pad, da qualunque formato arrivi.
bool loadInto (machine::Machine& m, int pad, const juce::File& file, const juce::String& name)
{
    if (file.hasFileExtension ("snd"))
    {
        juce::AudioBuffer<float> buffer;
        double rate = 44100.0;
        if (! readSnd (file, buffer, rate))
            return false;

        return m.engine().loadBuffer (pad, name, buffer, rate,
                                      sound::guessFromName (name.toUpperCase()));
    }

    return m.engine().loadFile (pad, file);
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

bool readSnd (const juce::File& file, juce::AudioBuffer<float>& destination, double& sampleRate)
{
    // L'intestazione, come la descrive la documentazione del formato:
    //   0x00  due byte 01 04
    //   0x02  16 lettere, il nome
    //   0x13  volume, 0x14 accordatura, 0x15 canali (0 mono, 1 stereo)
    //   0x16  inizio, 0x1a fine del giro, 0x1e quanti campioni, 0x22 durata
    //         del giro, tutti da quattro byte
    //   0x2a  comincia il suono, sedici bit con segno
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data) || data.getSize() < 64)
        return false;

    const auto* bytes = (const std::uint8_t*) data.getData();
    if (bytes[0] != 0x01 || bytes[1] != 0x04)
        return false;

    const int channels = bytes[0x15] == 1 ? 2 : 1;

    const auto readLong = [bytes] (int at)
    {
        return (int) (bytes[at] | (bytes[at + 1] << 8) | (bytes[at + 2] << 16)
                          | (bytes[at + 3] << 24));
    };

    constexpr int headerSize = 42;
    const int available = ((int) data.getSize() - headerSize) / (2 * channels);
    int frames = readLong (0x1e);

    // se il conto scritto non torna col file, ci si fida del file
    if (frames <= 0 || frames > available)
        frames = available;

    if (frames <= 0)
        return false;

    destination.setSize (channels, frames);

    const auto* samples = (const std::int16_t*) (bytes + headerSize);
    for (int i = 0; i < frames; ++i)
        for (int ch = 0; ch < channels; ++ch)
            destination.setSample (ch, i, (float) samples[i * channels + ch] / 32768.0f);

    sampleRate = 44100.0;      // le macchine di allora campionavano solo cosi'
    return true;
}

juce::File findSample (const juce::File& programFolder, const juce::String& name)
{
    if (name.isEmpty())
        return {};

    const juce::StringArray extensions { ".wav", ".WAV", ".aif", ".aiff", ".AIF", ".AIFF",
                                         ".flac", ".FLAC", ".snd", ".SND" };

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
    for (const auto& entry : juce::RangedDirectoryIterator (programFolder, true,
                                                            "*.wav;*.WAV;*.aif;*.aiff;*.snd;*.SND"))
        if (entry.getFile().getFileNameWithoutExtension().equalsIgnoreCase (name))
            return entry.getFile();

    return {};
}

// ============================================================================
// Il programma delle macchine vecchie (2000 e 2000XL): binario, non XML.
//
// Com'e' fatto, letto da un file vero e tornato con tre controlli diversi:
//
//   0x00  due byte 07 04, il marchio del formato
//   0x02  quanti campioni, contando anche la casella vuota numero zero
//   0x15  i nomi dei campioni: 16 lettere piu' uno zero, uno dietro l'altro
//         (numero di campioni meno uno, perche' lo zero e' il "niente")
//   poi   due byte, poi il nome del programma, 16 lettere
//   poi   trentadue byte, poi la tabella delle note: 64 record da 25 byte,
//         uno per nota dalla 35 alla 98. Il primo byte e' quale campione
//         suona quella nota, contando da uno; zero vuol dire nessuno
//   poi   due byte, poi 64 record da 6 byte: volume e panorama di ogni nota
//   poi   due byte, poi 64 byte: che nota fa ogni pad, dal primo al
//         sessantaquattresimo
//
// Il resto dei byte dentro i record non e' decodificato, e si lascia stare:
// meglio caricare un kit con qualche parametro in meno che non caricarlo.
// ============================================================================
Result loadOldProgram (const juce::File& pgm, machine::Machine& m)
{
    Result result;

    juce::MemoryBlock data;
    if (! pgm.loadFileAsData (data) || data.getSize() < 1000)
    {
        result.error = "NON LEGGO QUEL FILE";
        return result;
    }

    const auto* bytes = (const std::uint8_t*) data.getData();
    const int size = (int) data.getSize();

    if (bytes[0] != 0x07 || bytes[1] != 0x04)
    {
        result.error = "NON E' UN PROGRAMMA";
        return result;
    }

    const int declared = bytes[2] | (bytes[3] << 8);
    const int nameCount = juce::jlimit (0, 64, declared - 1);   // lo zero e' il "niente"

    const int namesStart = 0x15;
    const int programNameAt = namesStart + nameCount * 17 + 2;
    const int noteTableAt = programNameAt + 32;
    const int levelTableAt = noteTableAt + 64 * 25 + 2;
    const int padMapAt = levelTableAt + 64 * 6 + 2;

    if (padMapAt + 64 > size)
    {
        result.error = "NON E' UN PROGRAMMA";
        return result;
    }

    const auto textAt = [bytes] (int offset, int length)
    {
        return juce::String::fromUTF8 ((const char*) bytes + offset, length).trim();
    };

    juce::StringArray names;
    for (int i = 0; i < nameCount; ++i)
        names.add (textAt (namesStart + i * 17, 16));

    result.programName = textAt (programNameAt, 16);
    if (result.programName.isEmpty())
        result.programName = pgm.getFileNameWithoutExtension();

    const auto folder = pgm.getParentDirectory();

    // un kit prende il posto del kit: i 64 pad dei banchi da A a D si
    // svuotano, quelli da E a H restano come sono
    m.clearPads (0, 64);

    for (int pad = 0; pad < 64 && pad < machine::numDrums; ++pad)
    {
        int note = bytes[padMapAt + pad];
        if (note < 35 || note > 98)
            note = 35 + pad;                       // mappa rovinata: si va in fila

        const int slot = note - 35;
        const int sample = bytes[noteTableAt + slot * 25];
        if (sample <= 0 || sample > names.size())
            continue;                               // niente su questo pad

        const auto name = names[sample - 1];
        if (name.isEmpty())
            continue;

        const auto file = findSample (folder, name);
        if (file == juce::File() || ! loadInto (m, pad, file, name))
        {
            result.missing.add (name);
            continue;
        }

        auto& drum = m.engine().drum (pad);
        drum.name = name.toUpperCase();
        drum.kind = sound::guessFromName (drum.name);

        const int level = bytes[levelTableAt + slot * 6];
        const int pan = bytes[levelTableAt + slot * 6 + 1];

        if (level > 0 && level <= 200)
            drum.level = juce::jlimit (0.0f, 1.0f, (float) level / 100.0f);
        if (pan <= 100)
            drum.pan = juce::jlimit (-1.0f, 1.0f, (float) (pan - 50) / 50.0f);

        ++result.padsLoaded;
    }

    if (result.padsLoaded == 0 && result.error.isEmpty())
        result.error = "NESSUN CAMPIONE TROVATO";

    return result;
}

// ============================================================================
// Il programma dei modelli 1000, 2500 e 500.
//
// La struttura e' pubblicata per intero (Stephen Norum, mybunnyhug.org), e
// dice: intestazione con la dicitura "MPC1000 PGM 1.00", poi 64 blocchi da
// 0xA4 byte, uno per pad. Dentro ogni blocco stanno quattro strati da 0x18
// byte col nome del campione e il suo volume, e in fondo i parametri del pad:
// gruppo di taglio, volume, panorama, filtro.
// ============================================================================
Result loadThousandProgram (const juce::File& pgm, machine::Machine& m)
{
    Result result;

    juce::MemoryBlock data;
    if (! pgm.loadFileAsData (data) || data.getSize() < 0x2918 + 64)
    {
        result.error = "NON LEGGO QUEL FILE";
        return result;
    }

    const auto* bytes = (const std::uint8_t*) data.getData();
    const int size = (int) data.getSize();

    result.programName = pgm.getFileNameWithoutExtension();

    const auto folder = pgm.getParentDirectory();

    m.clearPads (0, 64);

    // i blocchi dei pad cominciano dopo l'intestazione, a 0x18: lo conferma
    // la sezione MIDI, che cade a 0x2918 proprio perche' 0x18 + 64 * 0xA4
    constexpr int padSection = 0x18;
    constexpr int padBlock = 0xA4;
    constexpr int layerBlock = 0x18;

    for (int pad = 0; pad < 64 && pad < machine::numDrums; ++pad)
    {
        const int base = padSection + pad * padBlock;
        if (base + padBlock > size)
            break;

        // il primo strato pieno: gli altri sono sovrapposizioni, e noi
        // suoniamo un campione per pad
        juce::String name;
        int layerAt = -1;

        for (int layer = 0; layer < 4; ++layer)
        {
            const int at = base + layer * layerBlock;
            const auto candidate = juce::String::fromUTF8 ((const char*) bytes + at, 16).trim();

            if (candidate.isNotEmpty())
            {
                name = candidate;
                layerAt = at;
                break;
            }
        }

        if (name.isEmpty())
            continue;

        const auto file = findSample (folder, name);
        if (file == juce::File() || ! loadInto (m, pad, file, name))
        {
            result.missing.add (name);
            continue;
        }

        auto& drum = m.engine().drum (pad);
        drum.name = name.toUpperCase();
        drum.kind = sound::guessFromName (drum.name);

        // volume e panorama del pad stanno in fondo al blocco
        const int mixerLevel = bytes[base + 0x8F];
        const int mixerPan = bytes[base + 0x90];

        if (mixerLevel <= 100)
            drum.level = juce::jlimit (0.0f, 1.0f, (float) mixerLevel / 100.0f);
        if (mixerPan <= 100)
            drum.pan = juce::jlimit (-1.0f, 1.0f, (float) (mixerPan - 50) / 50.0f);

        const int muteGroup = (std::int8_t) bytes[base + 0x63];
        if (muteGroup > 0)
            drum.muteGroup = muteGroup;

        // l'accordatura e' in centesimi di semitono, con segno
        if (layerAt >= 0 && layerAt + 0x16 < size)
        {
            const int cents = (std::int16_t) (bytes[layerAt + 0x14] | (bytes[layerAt + 0x15] << 8));
            drum.tuneSemitones = juce::jlimit (-24, 24, cents / 100);

            // 0 suona tutto, 1 suona finche' si tiene premuto
            drum.playMode = bytes[layerAt + 0x16] == 1 ? PlayMode::tenuto : PlayMode::unaVolta;
        }

        // il filtro: tipo e frequenza diventano la nostra manopola sola
        const int filterType = (std::int8_t) bytes[base + 0x71];
        const int filterFreq = bytes[base + 0x72];

        if (filterType == 1 && filterFreq <= 100)
            drum.filter = juce::jlimit (0.0f, 1.0f, (float) (100 - filterFreq) / 100.0f);
        else if (filterType == 3 && filterFreq <= 100)
            drum.filter = -juce::jlimit (0.0f, 1.0f, (float) filterFreq / 100.0f);

        ++result.padsLoaded;
    }

    if (result.padsLoaded == 0 && result.error.isEmpty())
        result.error = "NESSUN CAMPIONE TROVATO";

    return result;
}

Result loadProgram (const juce::File& file, machine::Machine& m)
{
    Result result;

    if (! file.existsAsFile())
    {
        result.error = "NON LEGGO QUEL FILE";
        return result;
    }

    // Il formato si riconosce da quello che c'e' dentro, non dal nome: i
    // modelli 1000, 2500 e 500 si firmano con una dicitura in chiaro, il 2000
    // e il 2000XL con due byte in testa.
    {
        juce::FileInputStream stream (file);
        if (stream.openedOk())
        {
            char head[20] = {};
            const int read = stream.read (head, 20);

            if (read >= 2 && (std::uint8_t) head[0] == 0x07 && (std::uint8_t) head[1] == 0x04)
                return loadOldProgram (file, m);

            if (read >= 20 && juce::String::fromUTF8 (head + 4, 16).startsWith ("MPC1000 PGM"))
                return loadThousandProgram (file, m);
        }
    }

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
    {
        result.error = "NON LEGGO QUEL FILE";
        return result;
    }

    const auto& xpm = file;

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

    m.clearPads (0, 64);

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

        if (! loadInto (m, pad, file, sampleName))
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
