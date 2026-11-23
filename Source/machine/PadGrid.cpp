#include "PadGrid.h"

#include <juce_core/juce_core.h>

namespace machine
{

static int quadrantOf (int rowFromTop, int columnFromLeft)
{
    const bool bottom = rowFromTop >= 4;
    const bool right  = columnFromLeft >= 4;
    // in basso a sinistra il primo, poi in basso a destra, poi la riga sopra
    return (bottom ? 0 : 2) + (right ? 1 : 0);
}

PadRef padForCell (int rowFromTop, int columnFromLeft, int bankGroup)
{
    jassert (juce::isPositiveAndBelow (rowFromTop, 8) && juce::isPositiveAndBelow (columnFromLeft, 8));

    const int quadrant = quadrantOf (rowFromTop, columnFromLeft);
    const int rowInQuadrant = rowFromTop % 4;        // 0 in alto
    const int colInQuadrant = columnFromLeft % 4;

    PadRef p;
    p.bank = bankGroup * banksOnScreen + quadrant;
    p.pad  = (3 - rowInQuadrant) * 4 + colInQuadrant + 1;   // il pad 1 sta in basso
    return p;
}

std::optional<std::pair<int, int>> cellForPad (PadRef p, int bankGroup)
{
    const int quadrant = p.bank - bankGroup * banksOnScreen;
    if (! juce::isPositiveAndBelow (quadrant, banksOnScreen))
        return std::nullopt;       // quel banco adesso non e' sulla griglia
    if (! juce::isPositiveAndBelow (p.pad - 1, padsPerBank))
        return std::nullopt;

    const int rowInQuadrant = 3 - (p.pad - 1) / 4;
    const int colInQuadrant = (p.pad - 1) % 4;

    const int row = (quadrant >= 2 ? 0 : 4) + rowInQuadrant;
    const int col = (quadrant % 2 == 1 ? 4 : 0) + colInQuadrant;
    return std::make_pair (row, col);
}

int drumIndex (PadRef p)
{
    return p.bank * padsPerBank + (p.pad - 1);
}

PadRef drumAt (int index)
{
    PadRef p;
    p.bank = index / padsPerBank;
    p.pad  = index % padsPerBank + 1;
    return p;
}

juce::String bankName (int bank)
{
    return juce::String::charToString ((juce::juce_wchar) ('A' + juce::jlimit (0, numBanks - 1, bank)));
}

} // namespace machine
