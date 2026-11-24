#pragma once

#include <optional>
#include <utility>

// La pagina PASSO: la meta' di sopra della griglia diventa lo step sequencer.
//
// Le quattro file in alto sono quattro tracce, una per fila, e otto passi, una
// per colonna: si vedono quattro suoni alla volta e si capisce al volo chi
// suona dove. I tasti scena a destra scorrono le tracce, i tasti pagina
// scorrono i passi, e una barra luminosa corre sulle colonne a tempo.
//
// Le quattro file in basso restano i pad dei due banchi di sotto, cosi' mentre
// si scrivono i passi si puo' ancora battere e scegliere il suono.
namespace machine
{

constexpr int stepRows = 4;             // quante tracce si vedono insieme
constexpr int stepColumns = 8;          // quanti passi si vedono insieme
constexpr int numSteps = stepColumns;

// La cella e' quella del Push: riga 0 in alto, colonna 0 a sinistra.
bool isStepCell (int rowFromTop);

// Che passo e' quella colonna, contando dalla finestra che si vede.
int stepForColumn (int columnFromLeft);

// Quale traccia e' quella fila, contando dalla prima che si vede.
int trackForRow (int rowFromTop);

} // namespace machine
