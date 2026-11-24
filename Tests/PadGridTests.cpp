#include "machine/PadGrid.h"

#include <juce_core/juce_core.h>

using namespace machine;

class PadGridTests final : public juce::UnitTest
{
public:
    PadGridTests() : juce::UnitTest ("Griglia dei pad", "Macchina") {}

    void runTest() override
    {
        beginTest ("i suoni stanno nelle quattro file di sotto, due banchi");
        {
            // riga 7 = in basso sul Push
            expect (padForCell (7, 0, 0) == PadRef { 0, 1 });    // A1, in basso a sinistra
            expect (padForCell (7, 4, 0) == PadRef { 1, 1 });    // B1
            expect (isSoundPad (padForCell (7, 0, 0)));
            expect (isSoundPad (padForCell (4, 7, 0)));

            // le quattro file di sopra sono le sequenze, non suoni
            expect (! isSoundPad (padForCell (3, 0, 0)));
            expect (! isSoundPad (padForCell (0, 7, 0)));
        }

        beginTest ("dentro il banco il pad 1 sta in basso a sinistra e il 16 in alto a destra");
        {
            expect (padForCell (7, 0, 0) == PadRef { 0, 1 });
            expect (padForCell (7, 3, 0) == PadRef { 0, 4 });
            expect (padForCell (4, 0, 0) == PadRef { 0, 13 });
            expect (padForCell (4, 3, 0) == PadRef { 0, 16 });
        }

        beginTest ("la seconda coppia sono i banchi C e D");
        {
            expect (padForCell (7, 0, 1) == PadRef { 2, 1 });    // C1
            expect (padForCell (7, 4, 1) == PadRef { 3, 1 });    // D1
            expect (padForCell (4, 7, 1) == PadRef { 3, 16 });   // D16
            expect (isSoundPad (padForCell (7, 0, 1)));

            expectEquals (bankName (2), juce::String ("C"));
            expectEquals (bankName (3), juce::String ("D"));

            // i 64 suoni stanno tutti nelle due coppie
            expectEquals (drumIndex (padForCell (7, 0, 0)), 0);
            expectEquals (drumIndex (padForCell (4, 7, 1)), soundDrums - 1);
        }

        beginTest ("andare e tornare dalla cella al pad");
        {
            for (int group = 0; group < 2; ++group)
                for (int row = 0; row < 8; ++row)
                    for (int col = 0; col < 8; ++col)
                    {
                        const auto p = padForCell (row, col, group);
                        const auto cell = cellForPad (p, group);
                        expect (cell.has_value());
                        expectEquals (cell->first, row);
                        expectEquals (cell->second, col);
                    }
        }

        beginTest ("un banco che non e' sulla griglia non ha cella");
        {
            expect (! cellForPad ({ 2, 1 }, 0).has_value(), "C non sta nella prima coppia");
            expect (! cellForPad ({ 0, 1 }, 1).has_value(), "A non sta nella seconda coppia");
        }

        beginTest ("l'indice del suono in memoria");
        {
            expectEquals (drumIndex ({ 0, 1 }), 0);
            expectEquals (drumIndex ({ 1, 1 }), 16);
            expectEquals (drumIndex ({ 7, 16 }), numDrums - 1);
            expect (drumAt (0) == PadRef { 0, 1 });
            expect (drumAt (numDrums - 1) == PadRef { 7, 16 });
        }
    }
};

static PadGridTests padGridTests;
