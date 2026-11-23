#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <utility>

// La griglia dei pad.
//
// Sulla macchina Akai i pad sono 16 e i banchi otto (A..H), numerati con il pad 1
// in basso a sinistra e il 16 in alto a destra. Il Push 2 di pad ne ha 64, quindi
// ne mostra quattro banchi alla volta, a quadranti, come sul pannello dell'MPC
// Sample dove i banchi sono scritti su due righe da quattro:
//
//     +-----------+-----------+        gruppo 0: A B C D
//     |     C     |     D     |        gruppo 1: E F G H
//     +-----------+-----------+
//     |     A     |     B     |
//     +-----------+-----------+
//
// Chi replica l'MPC60, che di suoni ne aveva 32, riempie A e B e si tiene liberi
// gli altri due quadranti per le fette o per i 16 livelli.
namespace machine
{

constexpr int numBanks = 8;
constexpr int padsPerBank = 16;
constexpr int numDrums = numBanks * padsPerBank;   // 128
constexpr int banksOnScreen = 4;

struct PadRef
{
    int bank = 0;   // 0..7, cioe' A..H
    int pad = 1;    // 1..16, il 1 in basso a sinistra come sulla macchina vera

    bool operator== (const PadRef& o) const { return bank == o.bank && pad == o.pad; }
};

// Dalla cella del Push al pad della macchina. Riga 0 = in alto, colonna 0 = a
// sinistra, come nel protocollo del Push. bankGroup 0 = A..D, 1 = E..H.
PadRef padForCell (int rowFromTop, int columnFromLeft, int bankGroup);

// L'inverso: dove sta quel pad sulla griglia, se il gruppo mostrato e' quello.
std::optional<std::pair<int, int>> cellForPad (PadRef, int bankGroup);

int drumIndex (PadRef);              // 0..127, l'indice del suono in memoria
PadRef drumAt (int index);
juce::String bankName (int bank);    // "A".."H"

} // namespace machine
