#include "machine/MpcProgram.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

// Il lettore dei programmi della macchina vera (.xpm).
//
// Il file e' XML, ma i nomi dei campi cambiano da una versione all'altra: qui
// si prova che il kit si carica lo stesso, che i pad vanno al posto giusto e
// che un campione che non si trova viene detto invece di far fallire tutto.
class MpcProgramTests final : public juce::UnitTest
{
public:
    MpcProgramTests() : juce::UnitTest ("Programmi MPC", "Macchina") {}

    static void writeWav (const juce::File& file, double seconds = 0.1)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        const int length = (int) (44100 * seconds);
        juce::AudioBuffer<float> b (1, length);
        for (int i = 0; i < length; ++i)
            b.setSample (0, i, 0.5f * (float) std::sin (0.05 * i));

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0));
        if (writer != nullptr)
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (b, 0, length);
        }
    }

    void runTest() override
    {
        auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("rebirth-xpm-"
                                           + juce::String (juce::Random::getSystemRandom().nextInt (100000)));
        temp.createDirectory();

        beginTest ("un programma mette i campioni sui pad giusti");
        {
            // i campioni stanno nella sottocartella, come nei pacchetti veri
            writeWav (temp.getChildFile ("Samples").getChildFile ("Kick One.wav"));
            writeWav (temp.getChildFile ("Samples").getChildFile ("Snare Two.wav"));
            writeWav (temp.getChildFile ("Samples").getChildFile ("Hat Three.wav"));

            const juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPCVObject>
  <Program type="Drum">
    <ProgramName>Kit di prova</ProgramName>
    <Instruments>
      <Instrument number="1">
        <Volume>0.8</Volume>
        <Pan>0.5</Pan>
        <MuteGroup>0</MuteGroup>
        <TriggerMode>One Shot</TriggerMode>
        <Layers>
          <Layer number="1"><SampleName>Kick One</SampleName></Layer>
          <Layer number="2"><SampleName></SampleName></Layer>
        </Layers>
      </Instrument>
      <Instrument number="2">
        <Volume>0.5</Volume>
        <Pan>0.75</Pan>
        <MuteGroup>3</MuteGroup>
        <TuneCoarse>-2</TuneCoarse>
        <TriggerMode>Note Off</TriggerMode>
        <Layers>
          <Layer number="1"><SampleName>Snare Two</SampleName></Layer>
        </Layers>
      </Instrument>
      <Instrument number="5">
        <Layers>
          <Layer number="1"><SampleName>Hat Three</SampleName></Layer>
        </Layers>
      </Instrument>
      <Instrument number="6">
        <Layers>
          <Layer number="1"><SampleName>Non Esisto</SampleName></Layer>
        </Layers>
      </Instrument>
    </Instruments>
  </Program>
</MPCVObject>)";

            const auto file = temp.getChildFile ("Kit di prova.xpm");
            file.replaceWithText (xml);

            machine::Machine m;
            m.prepare (44100.0);
            m.clearAllPads();

            const auto result = mpc::loadProgram (file, m);

            expect (result.ok(), "non ha caricato: " + result.error);
            expectEquals (result.padsLoaded, 3);
            expectEquals (result.programName, juce::String ("Kit di prova"));

            // il campione che non c'e' viene detto, non fa saltare il resto
            expectEquals (result.missing.size(), 1);
            expectEquals (result.missing[0], juce::String ("Non Esisto"));

            // strumento 1 sul pad 1, strumento 5 sul pad 5
            expectEquals (m.engine().drum (0).name, juce::String ("KICK ONE"));
            expectEquals (m.engine().drum (1).name, juce::String ("SNARE TWO"));
            expect (! m.engine().drum (2).loaded(), "il pad 3 doveva restare vuoto");
            expectEquals (m.engine().drum (4).name, juce::String ("HAT THREE"));

            // e i parametri che il file dice
            expectWithinAbsoluteError (m.engine().drum (1).level, 0.5f, 0.01f);
            expectWithinAbsoluteError (m.engine().drum (1).pan, 0.5f, 0.01f);
            expectEquals (m.engine().drum (1).muteGroup, 3);
            expectEquals (m.engine().drum (1).tuneSemitones, -2);
            expect (m.engine().drum (1).playMode == PlayMode::tenuto,
                    "Note Off doveva diventare TENUTO");

            // la famiglia si indovina dal nome, cosi' i colori tornano
            expect (m.engine().drum (0).kind == sound::Kind::cassa,
                    "il kick doveva essere riconosciuto come cassa");
        }

        beginTest ("i campioni si trovano anche se stanno li' accanto");
        {
            auto flat = temp.getChildFile ("piatto");
            flat.createDirectory();
            writeWav (flat.getChildFile ("Clap.wav"));

            const auto found = mpc::findSample (flat, "Clap");
            expect (found.existsAsFile(), "non trova il wav nella stessa cartella");
            expect (mpc::findSample (flat, "Niente") == juce::File());
        }

        beginTest ("un file che non e' un programma non fa danni");
        {
            machine::Machine m;
            m.prepare (44100.0);
            const int before = m.engine().loadedCount();

            auto bad = temp.getChildFile ("rotto.xpm");
            bad.replaceWithText ("questo non e' xml <<<");

            const auto result = mpc::loadProgram (bad, m);
            expect (! result.ok());
            expect (result.error.isNotEmpty());
            expectEquals (m.engine().loadedCount(), before, "ha toccato i pad per niente");

            // e un XML che non e' un programma
            auto other = temp.getChildFile ("altro.xpm");
            other.replaceWithText ("<Qualcosa><Dentro/></Qualcosa>");
            expect (! mpc::loadProgram (other, m).ok());

            // e un file che non c'e'
            expect (! mpc::loadProgram (temp.getChildFile ("mai-esistito.xpm"), m).ok());
        }

        beginTest ("un programma con i nomi dei campi diversi si carica lo stesso");
        {
            // le versioni vecchie scrivono SampleFile invece di SampleName, e
            // non hanno il numero sullo strumento
            writeWav (temp.getChildFile ("vecchio").getChildFile ("Tom.wav"));

            const juce::String xml = R"(<MPCVObject><Program>
  <Instruments>
    <Instrument><Layers><Layer><SampleFile>Tom</SampleFile></Layer></Layers></Instrument>
  </Instruments>
</Program></MPCVObject>)";

            const auto file = temp.getChildFile ("vecchio").getChildFile ("Vecchio.xpm");
            file.replaceWithText (xml);

            machine::Machine m;
            m.prepare (44100.0);
            m.clearAllPads();

            const auto result = mpc::loadProgram (file, m);
            expect (result.ok(), "il formato vecchio non si carica: " + result.error);
            expectEquals (m.engine().drum (0).name, juce::String ("TOM"));
        }

        beginTest ("un programma delle macchine vecchie (.pgm) mette i pad al posto giusto");
        {
            auto folder = temp.getChildFile ("vecchio-kit");
            folder.createDirectory();

            // tre campioni, coi nomi da sedici lettere come allora
            const juce::StringArray names { "KICK_UNO", "RIM_DUE", "HAT_TRE" };
            for (const auto& n : names)
                writeWav (folder.getChildFile (n + ".wav"));

            // Si costruisce un programma con la struttura del formato vero:
            // intestazione, nomi, nome del programma, tabella delle note,
            // volumi e panorami, e la mappa che dice che nota fa ogni pad.
            juce::MemoryBlock data;
            data.setSize (0x15, true);
            auto* head = (std::uint8_t*) data.getData();
            head[0] = 0x07;
            head[1] = 0x04;
            head[2] = (std::uint8_t) (names.size() + 1);    // la casella zero e' il niente

            const auto pad16 = [] (const juce::String& text)
            {
                auto s = text.substring (0, 16);
                while (s.length() < 16)
                    s += " ";
                return s;
            };

            for (const auto& n : names)
            {
                data.append (pad16 (n).toRawUTF8(), 16);
                data.append ("\0", 1);
            }

            const char marker[2] = { 0x1e, 0x00 };
            data.append (marker, 2);
            data.append (pad16 ("KIT VECCHIO").toRawUTF8(), 16);
            data.setSize (data.getSize() + 16, true);       // i sedici byte prima delle note

            // 64 note, dalla 35 alla 98: il primo byte e' il campione, da uno
            const size_t noteTable = data.getSize();
            data.setSize (noteTable + 64 * 25, true);
            auto* bytes = (std::uint8_t*) data.getData();
            bytes[noteTable + (36 - 35) * 25] = 1;          // nota 36: il primo campione
            bytes[noteTable + (37 - 35) * 25] = 2;
            bytes[noteTable + (42 - 35) * 25] = 3;

            // volumi e panorami
            const size_t levels = data.getSize() + 2;
            data.setSize (levels + 64 * 6, true);
            bytes = (std::uint8_t*) data.getData();
            for (int i = 0; i < 64; ++i)
            {
                bytes[levels + i * 6] = 100;
                bytes[levels + i * 6 + 1] = 50;
            }
            bytes[levels + (37 - 35) * 6] = 50;             // il rim a meta' volume
            bytes[levels + (42 - 35) * 6 + 1] = 100;        // il charleston tutto a destra

            // la mappa dei pad: come la macchina vera, A01 fa la nota 37
            const size_t padMap = data.getSize() + 2;
            data.setSize (padMap + 64, true);
            bytes = (std::uint8_t*) data.getData();
            // gli altri pad su note vuote: sulla macchina vera due pad non
            // possono fare la stessa nota
            for (int i = 0; i < 64; ++i)
                bytes[padMap + i] = (std::uint8_t) (50 + (i % 48));
            bytes[padMap + 0] = 37;
            bytes[padMap + 1] = 36;
            bytes[padMap + 2] = 42;

            const auto file = folder.getChildFile ("KIT VECCHIO.PGM");
            file.replaceWithData (data.getData(), data.getSize());

            machine::Machine m;
            m.prepare (44100.0);
            m.clearAllPads();

            const auto result = mpc::loadProgram (file, m);    // riconosciuto dai primi due byte

            expect (result.ok(), "non ha caricato: " + result.error);
            expectEquals (result.programName, juce::String ("KIT VECCHIO"));
            expectEquals (result.padsLoaded, 3);

            expectEquals (m.engine().drum (0).name, juce::String ("RIM_DUE"));
            expectEquals (m.engine().drum (1).name, juce::String ("KICK_UNO"));
            expectEquals (m.engine().drum (2).name, juce::String ("HAT_TRE"));
            expect (! m.engine().drum (3).loaded(), "il quarto pad doveva restare vuoto");

            expectWithinAbsoluteError (m.engine().drum (0).level, 0.5f, 0.02f);
            expectWithinAbsoluteError (m.engine().drum (2).pan, 1.0f, 0.02f);
        }

        beginTest ("un .pgm tagliato a meta' non fa danni");
        {
            machine::Machine m;
            m.prepare (44100.0);

            auto bad = temp.getChildFile ("corto.pgm");
            juce::MemoryBlock small;
            small.setSize (1200, true);
            auto* b = (std::uint8_t*) small.getData();
            b[0] = 0x07;
            b[1] = 0x04;
            b[2] = 40;                                   // dice quaranta campioni ma il file e' corto
            bad.replaceWithData (small.getData(), small.getSize());

            const auto result = mpc::loadProgram (bad, m);
            expect (! result.ok());
            expect (result.error.isNotEmpty());
        }

        beginTest ("un programma dei modelli 1000 e 2500 si carica");
        {
            auto folder = temp.getChildFile ("mille");
            folder.createDirectory();

            const juce::StringArray names { "BD_MILLE", "SD_MILLE", "HH_MILLE" };
            for (const auto& n : names)
                writeWav (folder.getChildFile (n + ".wav"));

            // La struttura pubblicata: intestazione con la dicitura, poi 64
            // blocchi da 0xA4 byte, uno per pad, con quattro strati da 0x18.
            juce::MemoryBlock data;
            data.setSize (0x2A04, true);
            auto* b = (std::uint8_t*) data.getData();

            b[0] = 0x04;                       // la dimensione, in fondo alla parola
            b[1] = 0x2A;
            memcpy (b + 4, "MPC1000 PGM 1.00", 16);

            const int padSection = 0x18;      // i pad cominciano dopo l'intestazione

            const auto putPad = [b, &names, padSection] (int pad, int which, int level, int pan,
                                                         int muteGroup, int cents, int playMode)
            {
                const int base = padSection + pad * 0xA4;
                const auto name = names[which];
                memcpy (b + base, name.toRawUTF8(), (size_t) name.length());

                b[base + 0x14] = (std::uint8_t) (cents & 0xff);          // accordatura
                b[base + 0x15] = (std::uint8_t) ((cents >> 8) & 0xff);
                b[base + 0x16] = (std::uint8_t) playMode;
                b[base + 0x63] = (std::uint8_t) muteGroup;
                b[base + 0x8F] = (std::uint8_t) level;
                b[base + 0x90] = (std::uint8_t) pan;
            };

            putPad (0, 0, 100, 50, 0, 0, 0);                 // cassa, dritta
            putPad (1, 1, 60, 25, 2, -1200, 1);              // rullante piano, a sinistra,
                                                             // un'ottava sotto, tenuto
            putPad (2, 2, 100, 50, 1, 0, 0);

            // un filtro passa basso sul terzo pad
            b[padSection + 2 * 0xA4 + 0x71] = 1;
            b[padSection + 2 * 0xA4 + 0x72] = 40;

            const auto file = folder.getChildFile ("MILLE.PGM");
            file.replaceWithData (data.getData(), data.getSize());

            machine::Machine m;
            m.prepare (44100.0);

            const auto result = mpc::loadProgram (file, m);   // riconosciuto dalla dicitura

            expect (result.ok(), "non ha caricato: " + result.error);
            expectEquals (result.padsLoaded, 3);

            expectEquals (m.engine().drum (0).name, juce::String ("BD_MILLE"));
            expectEquals (m.engine().drum (1).name, juce::String ("SD_MILLE"));
            expectEquals (m.engine().drum (2).name, juce::String ("HH_MILLE"));
            expect (! m.engine().drum (3).loaded());

            const auto& snare = m.engine().drum (1);
            expectWithinAbsoluteError (snare.level, 0.6f, 0.02f);
            expectWithinAbsoluteError (snare.pan, -0.5f, 0.02f);
            expectEquals (snare.muteGroup, 2);
            expectEquals (snare.tuneSemitones, -12, "l'accordatura e' in centesimi");
            expect (snare.playMode == PlayMode::tenuto);

            // il filtro passa basso diventa la nostra manopola girata a destra
            expect (m.engine().drum (2).filter > 0.5f,
                    "il filtro non e' arrivato: " + juce::String (m.engine().drum (2).filter));
        }

        beginTest ("caricare un kit svuota i pad di prima");
        {
            auto folder = temp.getChildFile ("ricambio");
            folder.createDirectory();
            writeWav (folder.getChildFile ("SOLO_UNO.wav"));

            juce::MemoryBlock data;
            data.setSize (0x2A04, true);
            auto* b = (std::uint8_t*) data.getData();
            b[0] = 0x04;
            b[1] = 0x2A;
            memcpy (b + 4, "MPC1000 PGM 1.00", 16);
            memcpy (b + 0x18, "SOLO_UNO", 8);      // il primo pad

            const auto file = folder.getChildFile ("RICAMBIO.PGM");
            file.replaceWithData (data.getData(), data.getSize());

            machine::Machine m;
            m.prepare (44100.0);
            expect (m.engine().loadedCount() > 1, "il kit di partenza non c'e'");

            const auto result = mpc::loadProgram (file, m);
            expect (result.ok(), result.error);

            // il kit di partenza non deve restare sotto quello nuovo
            expectEquals (m.engine().loadedCount(), 1, "sono rimasti i suoni di prima");
            expectEquals (m.engine().drum (0).name, juce::String ("SOLO_UNO"));
        }

        beginTest ("i campioni .SND delle macchine vecchie si leggono");
        {
            // l'intestazione del formato: 01 04, il nome, i canali, quanti
            // campioni, e poi il suono a sedici bit
            const int frames = 1000;
            juce::MemoryBlock data;
            data.setSize (42, true);
            auto* b = (std::uint8_t*) data.getData();
            b[0] = 0x01;
            b[1] = 0x04;
            b[0x15] = 0;                                 // mono
            b[0x1e] = (std::uint8_t) (frames & 0xff);
            b[0x1f] = (std::uint8_t) ((frames >> 8) & 0xff);

            std::vector<std::int16_t> samples ((size_t) frames);
            for (int i = 0; i < frames; ++i)
                samples[(size_t) i] = (std::int16_t) (16000 * std::sin (0.05 * i));
            data.append (samples.data(), samples.size() * sizeof (std::int16_t));

            auto file = temp.getChildFile ("VECCHIO.SND");
            file.replaceWithData (data.getData(), data.getSize());

            juce::AudioBuffer<float> buffer;
            double rate = 0.0;
            expect (mpc::readSnd (file, buffer, rate), "non legge il .SND");
            expectEquals (buffer.getNumSamples(), frames);
            expectEquals (buffer.getNumChannels(), 1);
            expectWithinAbsoluteError (rate, 44100.0, 0.1);
            expect (buffer.getMagnitude (0, frames) > 0.4f, "il suono e' muto");

            // un file che non e' un .SND
            auto bad = temp.getChildFile ("NIENTE.SND");
            bad.replaceWithText ("non sono un suono");
            juce::AudioBuffer<float> other;
            expect (! mpc::readSnd (bad, other, rate));
        }

        temp.deleteRecursively();
    }
};

static MpcProgramTests mpcProgramTests;
