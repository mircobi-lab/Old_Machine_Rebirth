#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <utility>

// La griglia dei pad.
//
// Sulla macchina Akai i pad sono 16 e i banchi otto (A..H), numerati con il pad 1
// in basso a sinistra e il 16 in alto a destra.
//
// Sul Push la griglia e' divisa in due per sempre: le quattro file in basso
// sono i suoni, due banchi da sedici, e le quattro file in alto sono le
// sequenze. Il tasto dei banchi cambia la coppia di sotto:
//
//     +-----------+-----------+     file 0-3: le sequenze, non sono pad
//     |  sequenze | sequenze  |
//     +-----------+-----------+     coppia 0: A a sinistra, B a destra
//     |     A     |     B     |     coppia 1: C a sinistra, D a destra
//     +-----------+-----------+
//
// Cosi' un kit da 32 suoni sta tutto sotto le dita, e uno da 64 si divide fra
// le due coppie: i primi 32 nella prima, gli altri nella seconda.
namespace machine
{

constexpr int numBanks = 8;
constexpr int padsPerBank = 16;
constexpr int numDrums = numBanks * padsPerBank;   // 128
constexpr int banksOnScreen = 2;        // i banchi di suoni che si vedono insieme
constexpr int soundBanks = 4;           // A B C D: i banchi che tengono i suoni
constexpr int soundDrums = soundBanks * padsPerBank;   // 64

struct PadRef
{
    int bank = 0;   // 0..7, cioe' A..H
    int pad = 1;    // 1..16, il 1 in basso a sinistra come sulla macchina vera

    bool operator== (const PadRef& o) const { return bank == o.bank && pad == o.pad; }
};

// Dalla cella del Push al pad della macchina. Riga 0 = in alto, colonna 0 = a
// sinistra, come nel protocollo del Push. bankGroup 0 = A e B, 1 = C e D.
//
// Le file in alto non sono suoni: tengono le sequenze. Hanno comunque un pad
// loro, nei banchi da E a H, cosi' chi passa una cella puo' sempre dire di
// quale si tratta; quei banchi non ricevono mai un campione.
PadRef padForCell (int rowFromTop, int columnFromLeft, int bankGroup);

// Vero se quel pad e' un suono, cioe' se sta nella meta' di sotto.
bool isSoundPad (PadRef);

// L'inverso: dove sta quel pad sulla griglia, se il gruppo mostrato e' quello.
std::optional<std::pair<int, int>> cellForPad (PadRef, int bankGroup);

int drumIndex (PadRef);              // 0..127, l'indice del suono in memoria
PadRef drumAt (int index);
juce::String bankName (int bank);    // "A".."H"

} // namespace machine
