# Old Machine Rebirth

Una macchina da campionamento che gira sul Mac e si suona dall'Ableton Push 2.

Il Push 3 ha mandato in pensione il Push 2, che pero' di vecchio non ha niente:
64 pad RGB sensibili al tocco, otto rotativi che sentono il dito, una striscia,
i pedali e un display a colori da 960x160. Attaccato a un Mac, che ci mette
potenza di calcolo, ingressi, uscite, microfono e casse, diventa il pannello di
una macchina vera, sul modello dell'Akai MPC60 e della sua lettura moderna.

Il progetto e' descritto in
[docs/superpowers/specs](docs/superpowers/specs/2026-11-23-macchina-campionatore-design.md):
cosa fa, cosa non fa, il pannello sul Push e l'ordine dei lavori.

## A che punto e' (23 novembre 2026)

Fatto:

- la finestra in stile MBStudio, con la griglia 8x8 a quadranti e la schermata
  del Push rifatta anche sul Mac
- il Push 2 si accende da solo: si mette in User mode, lascia libera la porta
  Live per Ableton, illumina pad e tasti e scrive sul display
- tutti i 65 tasti del Push hanno un comando, con una prova automatica che
  tiene il conto: vedi [docs/pannello-push2.md](docs/pannello-push2.md)
- la versione senza Push: la macchina si suona in OSC con gli stessi comandi, e
  c'e' un banco pronto per Open Stage Control: vedi [docs/osc.md](docs/osc.md)
- la scheda audio si sceglie dalla finestra e battendo un pad si sente un colpo
  di prova, giusto per dire che la catena audio e' aperta

Da fare, nell'ordine: voci e suoni sui pad, campionamento con soglia e taglio,
affettatura, sequencer, passo passo e brano, mix. L'ordine completo sta nel
documento di progetto.

## La griglia

Sulla macchina Akai i pad sono 16 e i banchi otto. Il Push ne ha 64, quindi ne
mostra quattro banchi alla volta, a quadranti:

```
+-----------+-----------+     il tasto dei banchi passa
|     C     |     D     |     al gruppo E F G H
+-----------+-----------+
|     A     |     B     |     dentro ogni banco il pad 1
+-----------+-----------+     sta in basso a sinistra
```

## Compilare

La compilazione si fa su GitHub Actions a ogni push, e l'app pronta si scarica
dagli artefatti della build. In locale:

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Serve CMake 3.22 e un compilatore C++17; JUCE 8.0.15 e libusb vengono scaricati
da soli.

## Com'e' fatto

- `Source/machine` il modello: la griglia dei pad, e presto i suoni e le sequenze
- `Source/push2` il protocollo del Push 2 e il suo display via USB
- `Source/control` il Push come pannello: porta User, LED, display
- `Source/ui` la finestra sul Mac, in stile MBStudio
- `Tests` le prove, che girano a ogni push

Il codice di `Source/push2` nasce in
[MBStudio91-Stage](https://github.com/mircobi-lab/MBStudio91-Stage): se cambia
di la', va riportato qui.

## Licenze

Font Bebas Neue con licenza SIL OFL, copia in `Assets/Fonts`. Il marchio
MBSTUDIO91 e' di MBStudio91. Nessun marchio di terzi: la macchina si ispira al
modo di lavorare dell'MPC60, non ne copia nomi, loghi o grafica.
