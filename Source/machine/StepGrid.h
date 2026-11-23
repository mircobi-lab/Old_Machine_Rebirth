#pragma once

#include <optional>
#include <utility>

// La pagina PASSO: la meta' di sopra della griglia diventa lo step sequencer.
//
// Le quattro file in alto sono 32 passi, da sinistra a destra e dall'alto in
// basso: il passo 1 in alto a sinistra, il 32 in fondo a destra della quarta
// fila. Le quattro file in basso restano i pad dei due banchi di sotto, cosi'
// mentre si scrivono i passi si puo' ancora battere e scegliere il suono da
// scrivere.
namespace machine
{

constexpr int stepRows = 4;
constexpr int stepColumns = 8;
constexpr int numSteps = stepRows * stepColumns;      // 32

// La cella e' quella del Push: riga 0 in alto, colonna 0 a sinistra.
bool isStepCell (int rowFromTop);

// Che passo e' quella cella, 0..31. -1 se la cella non e' nella meta' di sopra.
int stepForCell (int rowFromTop, int columnFromLeft);

// L'inverso.
std::optional<std::pair<int, int>> cellForStep (int step);

} // namespace machine
