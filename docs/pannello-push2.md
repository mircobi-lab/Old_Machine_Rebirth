# Il Push 2 come pannello

Tutti i 65 tasti del Push, gli 11 rotativi e la striscia hanno un lavoro. I
numeri vengono dalla mappa ufficiale di Ableton (`Push2-map.json` e
`MidiMapping.png` di Ableton/push-interface). Una prova automatica controlla che
non resti fuori nessun tasto: se il Push ne ha uno in piu' di quelli mappati, i
test falliscono.

## Sopra il display, le otto pagine (cc 102-109)

CAMPIONA, FETTE, SUONO, SEQUENZA, PASSO, BRANO, MIX, DISCO. La pagina aperta sta
accesa forte, le altre smorte.

## Sotto il display, gli otto tasti funzione (cc 20-27)

Cambiano nome con la pagina, come i quattro tasti sotto lo schermo dell'MPC60.
Qui sono il doppio.

## I rotativi

| Controllo | cc | Lavoro |
|---|---|---|
| Primo a sinistra | 14 | Tempo |
| Secondo a sinistra | 15 | Swing |
| Gli otto in fila | 71-78 | Gli otto parametri scritti sul display |
| Master | 79 | Volume generale |

Toccarli, senza girarli, serve a prendere il parametro senza cambiarlo.

## La colonna a destra dei pad (cc 43-36)

E' il valore della quantizzazione, come c'e' scritto sul Push: dall'alto 1/32T,
1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4T, 1/4. Vale per CORREGGI e per il note repeat.

## Trasporto e registrazione

| Tasto sul Push | cc | Lavoro |
|---|---|---|
| Play | 85 | Parte e riparte |
| Record | 86 | Registra la sequenza |
| Stop Clip | 29 | Ferma |
| Automate | 89 | Sovraincide |
| Fixed Length | 90 | Quante battute |
| New | 87 | Sequenza nuova |
| Duplicate | 88 | Copia |
| Quantize | 116 | Correggi i tempi |
| Double Loop | 117 | Raddoppia la sequenza |
| Convert | 35 | Ricampiona quello che esce |

## Suonare

| Tasto | cc | Lavoro |
|---|---|---|
| Repeat | 56 | Note repeat, al valore scelto nella colonna |
| Accent | 57 | Full level |
| Scale | 58 | 16 livelli |
| Layout | 31 | Cambia disposizione dei pad, A-D o E-H |
| Note | 50 | Modo suona |
| Session | 51 | Modo sequenze |
| Octave su e giu' | 55, 54 | Accorda il pad scelto |
| Mute, Solo | 60, 61 | Muta e isola |
| Select | 48 | Sceglie un pad senza suonarlo |
| Shift | 49 | Seconda funzione |

## Spostarsi e servizio

| Tasto | cc | Lavoro |
|---|---|---|
| Frecce | 46, 47, 44, 45 | Muoversi fra i campi |
| Page sinistra e destra | 62, 63 | Battuta prima e dopo |
| Delete | 118 | Cancella |
| Undo | 119 | Annulla |
| Tap Tempo | 3 | Batte il tempo |
| Metronome | 9 | Metronomo |
| Setup | 30 | Scheda audio e MIDI |
| User | 59 | Ridai il Push ad Ableton |
| Master | 28 | Volume generale |
| Device | 110 | Parametri del pad |
| Mix | 112 | Mixer |
| Browse | 111 | Sfoglia i campioni |
| Clip | 113 | Elenco delle sequenze |
| Add Device | 52 | Aggiungi un effetto al pad |
| Add Track | 53 | Aggiungi una traccia |

## La striscia

E' il fader dell'MPC, assegnabile a un parametro, come sull'MPC Sample.

## I pad

Quattro banchi per volta a quadranti, il pad 1 in basso a sinistra di ogni
quadrante; Layout passa al gruppo E-H. Vedi il README.
