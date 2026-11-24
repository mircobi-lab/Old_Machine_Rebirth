#include "PadGrid.h"

#include <juce_core/juce_core.h>

namespace machine
{

PadRef padForCell (int rowFromTop, int columnFromLeft, int bankGroup)
{
    jassert (juce::isPositiveAndBelow (rowFromTop, 8) && juce::isPositiveAndBelow (columnFromLeft, 8));

    const bool top = rowFromTop < 4;
    const bool right = columnFromLeft >= 4;
    const int rowInQuadrant = rowFromTop % 4;        // 0 in alto
    const int colInQuadrant = columnFromLeft % 4;

    PadRef p;

    // in basso i suoni (A B oppure C D), in alto le sequenze, che si portano
    // dietro un banco da E a H per non restare senza nome
    p.bank = (top ? soundBanks : 0) + bankGroup * banksOnScreen + (right ? 1 : 0);
    p.pad  = (3 - rowInQuadrant) * 4 + colInQuadrant + 1;   // il pad 1 sta in basso
    return p;
}

bool isSoundPad (PadRef p)
{
    return juce::isPositiveAndBelow (p.bank, soundBanks);
}

std::optional<std::pair<int, int>> cellForPad (PadRef p, int bankGroup)
{
    if (! juce::isPositiveAndBelow (p.pad - 1, padsPerBank))
        return std::nullopt;

    const bool top = p.bank >= soundBanks;
    const int side = (p.bank - (top ? soundBanks : 0)) - bankGroup * banksOnScreen;

    if (! juce::isPositiveAndBelow (side, banksOnScreen))
        return std::nullopt;       // quel banco adesso non e' sulla griglia

    const int rowInQuadrant = 3 - (p.pad - 1) / 4;
    const int colInQuadrant = (p.pad - 1) % 4;

    const int row = (top ? 0 : 4) + rowInQuadrant;
    const int col = (side == 1 ? 4 : 0) + colInQuadrant;
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
