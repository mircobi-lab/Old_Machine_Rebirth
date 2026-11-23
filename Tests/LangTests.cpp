#include "machine/Machine.h"
#include "ui/Lang.h"

#include <juce_core/juce_core.h>

// Le tre lingue: si controlla che il display cambi davvero e che niente
// scompaia per strada, perche' una parola senza traduzione deve restare quella
// italiana invece di lasciare la casella vuota.
class LangTests final : public juce::UnitTest
{
public:
    LangTests() : juce::UnitTest ("Lingue", "Interfaccia") {}

    void runTest() override
    {
        beginTest ("in italiano le parole restano quelle scritte nel codice");
        {
            lang::set (lang::Language::italiano);
            expectEquals (lang::t ("SPACCA"), juce::String ("SPACCA"));
            expectEquals (lang::phrase ("PASSO 1/16"), juce::String ("PASSO 1/16"));
        }

        beginTest ("inglese e francese cambiano le parole del pannello");
        {
            lang::set (lang::Language::english);
            expectEquals (lang::t ("CAMPIONA"), juce::String ("SAMPLE"));
            expectEquals (lang::t ("PULISCI MP3"), juce::String ("CLEAN MP3"));

            lang::set (lang::Language::francais);
            expectEquals (lang::t ("CAMPIONA"), juce::String ("ECHANTILLON"));
            expectEquals (lang::t ("CANCELLA"), juce::String ("EFFACER"));
        }

        beginTest ("nelle frasi i numeri e i nomi passano intatti");
        {
            lang::set (lang::Language::english);
            expectEquals (lang::phrase ("PASSO 1/16"), juce::String ("STEP 1/16"));
            expectEquals (lang::phrase ("LIVELLO 80"), juce::String ("LEVEL 80"));
            expectEquals (lang::phrase ("ANNULLATO FETTE"), juce::String ("UNDONE CHOP"));
            expectEquals (lang::phrase ("16 LIVELLI ACCESO"), juce::String ("16 LEVELS ON"));
            expectEquals (lang::phrase ("PULITO AI"), juce::String ("CLEANED AI"));
        }

        beginTest ("una parola senza traduzione resta in italiano");
        {
            lang::set (lang::Language::english);
            expectEquals (lang::t ("PAROLA CHE NON ESISTE"), juce::String ("PAROLA CHE NON ESISTE"));
            expectEquals (lang::phrase ("LIVELLO BOOM 99"), juce::String ("LEVEL BOOM 99"));
        }

        beginTest ("il display della macchina cambia lingua");
        {
            machine::Machine m;
            m.prepare (44100.0);

            // la pagina SUONO: li' il display mostra i parametri del campione
            for (const auto& c : control::buttons())
                if (c.command == control::Command::pageSound)
                {
                    m.command (c, true);
                    m.command (c, false);
                }

            lang::set (lang::Language::italiano);
            const auto italiano = m.screenLines();
            expect (italiano.joinIntoString (" ").contains ("LIVELLO"));

            lang::set (lang::Language::english);
            const auto english = m.screenLines();
            expect (english.joinIntoString (" ").contains ("LEVEL"));
            expect (! english.joinIntoString (" ").contains ("LIVELLO"));
            expectEquals (english.size(), italiano.size());

            lang::set (lang::Language::francais);
            const auto francais = m.screenLines();
            expect (francais.joinIntoString (" ").contains ("NIVEAU"));

            // sul display non ci devono finire lettere accentate: il francese
            // del pannello e' in maiuscolo senza accenti
            for (const auto& line : francais)
                for (int i = 0; i < line.length(); ++i)
                    expect (line[i] < 128, "carattere fuori dall'ascii: " + line);

            lang::set (lang::Language::italiano);
        }

        beginTest ("i tasti sotto il display seguono la lingua");
        {
            machine::Machine m;
            m.prepare (44100.0);

            control::Control chop;
            for (const auto& c : control::buttons())
                if (c.command == control::Command::pageChop)
                    chop = c;

            m.command (chop, true);
            m.command (chop, false);

            lang::set (lang::Language::francais);
            expectEquals (m.softKeyLabels()[0], juce::String ("DECOUPER"));
            lang::set (lang::Language::italiano);
            expectEquals (m.softKeyLabels()[0], juce::String ("SPACCA"));
        }
    }
};

static LangTests langTests;
