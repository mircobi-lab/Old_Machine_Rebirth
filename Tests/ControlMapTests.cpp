#include "control/ControlMap.h"

#include <juce_core/juce_core.h>
#include <set>

// I numeri di questa lista sono quelli della mappa ufficiale di Ableton
// (doc/Push2-map.json, 65 tasti). Se un tasto non ha un comando, questa prova
// lo dice: la macchina deve usare tutto quello che il Push ha.
static const int officialButtonCCs[] = {
    3, 9, 118, 119, 60, 61, 29, 35, 117, 116, 88, 87, 90, 89, 86, 85,
    102, 103, 104, 105, 106, 107, 108, 109,
    20, 21, 22, 23, 24, 25, 26, 27,
    43, 42, 41, 40, 39, 38, 37, 36,
    30, 59, 52, 53, 110, 112, 111, 113, 28,
    46, 47, 44, 45, 56, 57, 58, 31, 50, 51, 55, 54, 62, 63, 49, 48
};

class ControlMapTests final : public juce::UnitTest
{
public:
    ControlMapTests() : juce::UnitTest ("Mappa dei comandi", "Macchina") {}

    void runTest() override
    {
        beginTest ("tutti i 65 tasti del Push hanno un comando");
        {
            expectEquals ((int) (sizeof (officialButtonCCs) / sizeof (int)), 65);

            for (const int cc : officialButtonCCs)
            {
                const auto c = control::forButton (cc);
                expect (c.command != control::Command::none, "tasto senza comando: cc" + juce::String (cc));
                expect (c.label.isNotEmpty(), "tasto senza nome: cc" + juce::String (cc));
            }
        }

        beginTest ("nessun tasto e' mappato due volte, e nessuno e' inventato");
        {
            std::set<int> seen, official (std::begin (officialButtonCCs), std::end (officialButtonCCs));
            for (const auto& c : control::buttons())
            {
                expect (seen.insert (c.number).second, "cc doppio: " + juce::String (c.number));
                expect (official.count (c.number) == 1, "cc che il Push non ha: " + juce::String (c.number));
            }
            expectEquals ((int) control::buttons().size(), 65);
        }

        beginTest ("gli undici rotativi");
        {
            expectEquals (control::forEncoder (14).label, juce::String ("TEMPO"));
            expectEquals (control::forEncoder (15).label, juce::String ("SWING"));
            expectEquals (control::forEncoder (79).label, juce::String ("VOLUME"));
            for (int i = 0; i < 8; ++i)
            {
                const auto e = control::forEncoder (71 + i);
                expect (e.command == control::Command::parameter);
                expectEquals (e.index, i);
            }
            expectEquals ((int) control::encoders().size(), 11);
        }

        beginTest ("la colonna a destra dei pad e' la quantizzazione, dal fine al largo");
        {
            // dall'alto: 1/32t fino a 1/4, come c'e' scritto sul Push
            expectEquals (control::forButton (43).label, juce::String ("1/32T"));
            expectEquals (control::forButton (36).label, juce::String ("1/4"));
            expectEquals (control::forButton (43).index, 0);
            expectEquals (control::forButton (36).index, 7);
            expectEquals (control::quantiseName (0), juce::String ("1/32T"));
            expectEquals (control::quantiseName (7), juce::String ("1/4"));
        }

        beginTest ("un CC che non e' un tasto non da' comandi");
        {
            expect (control::forButton (1).command == control::Command::none);
            expect (control::forButton (127).command == control::Command::none);
        }
    }
};

static ControlMapTests controlMapTests;
