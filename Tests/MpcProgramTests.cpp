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
            expect (m.engine().drum (0).kind == sound::Kind::batteria);
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

        temp.deleteRecursively();
    }
};

static MpcProgramTests mpcProgramTests;
