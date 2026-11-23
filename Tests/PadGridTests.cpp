#include "machine/PadGrid.h"

#include <juce_core/juce_core.h>

using namespace machine;

class PadGridTests final : public juce::UnitTest
{
public:
    PadGridTests() : juce::UnitTest ("Griglia dei pad", "Macchina") {}

    void runTest() override
    {
        beginTest ("i quattro quadranti sono quattro banchi");
        {
            // riga 7 = in basso sul Push
            expect (padForCell (7, 0, 0) == PadRef { 0, 1 });    // A1, in basso a sinistra
            expect (padForCell (7, 4, 0) == PadRef { 1, 1 });    // B1
            expect (padForCell (3, 0, 0) == PadRef { 2, 1 });    // C1
            expect (padForCell (3, 4, 0) == PadRef { 3, 1 });    // D1
        }

        beginTest ("dentro il banco il pad 1 sta in basso a sinistra e il 16 in alto a destra");
        {
            expect (padForCell (7, 0, 0) == PadRef { 0, 1 });
            expect (padForCell (7, 3, 0) == PadRef { 0, 4 });
            expect (padForCell (4, 0, 0) == PadRef { 0, 13 });
            expect (padForCell (4, 3, 0) == PadRef { 0, 16 });
        }

        beginTest ("il secondo gruppo sono i banchi da E a H");
        {
            expect (padForCell (7, 0, 1) == PadRef { 4, 1 });    // E1
            expect (padForCell (0, 7, 1) == PadRef { 7, 16 });   // H16
            expectEquals (bankName (4), juce::String ("E"));
            expectEquals (bankName (7), juce::String ("H"));
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
            expect (! cellForPad ({ 5, 1 }, 0).has_value());
            expect (! cellForPad ({ 0, 1 }, 1).has_value());
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
