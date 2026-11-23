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
Qui sono il doppio. Quelli che in una pagina non servono restano spenti.

| Pagina | F1 | F2 | F3 | F4 | F5 | F6 | F7 |
|---|---|---|---|---|---|---|---|
| CAMPIONA | ARMA | REGISTRA | STOP | ASCOLTA | RICAMPIONA | IN LIBRERIA | ANNULLA |
| FETTE | SPACCA | PULISCI AI | PULISCI MP3 | ANNULLA | RIEMPI | FETTE + | FETTE - |
| SEQUENZA | REC SEQ | SOVRAINCIDI | CORREGGI | CANCELLA SEQ | NUOVA SEQ | RADDOPPIA | DIMEZZA |
| PASSO | CANCELLA SUONO | PASSI - | PASSI + | CORREGGI | | | |
| DISCO | SALVA SCENA | APRI SCENA | IN LIBRERIA | LIBRERIA | | | |

## La striscia sotto il display

Manda il pitch bend e muove il volume generale: giu' tutto in basso, su tutto
in cima.

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

## I colori dei pad

Il colore dice di che famiglia e' il suono, cosi' guardando la griglia si capisce
dove sta la batteria e dove stanno le voci senza leggere il display. Sono le
tinte dello stile MBStudio: grigi, blu, celeste e verde scuro.

| Famiglia | Colore |
|---|---|
| Batteria | grigio |
| Percussioni | grigio bluastro |
| Voce | celeste |
| Giri e fette | blu |
| Accordi | verde scuro |
| Basso | blu notte |
| Registrato | grigio chiaro |
| Senza famiglia | grigio scuro |

Ogni famiglia ha tre intensita': il pad fermo e' smorto, quello scelto e' pieno,
quello che suona tira al bianco. Il pad vuoto e' quasi spento e quello mutato e'
rosso, e quei due non dipendono dalla famiglia. In tutto sono 26 voci della
tavolozza del Push, scritte all'accensione dopo essersi segnati quelle
originali, che vengono rimesse quando si chiude.

La famiglia la indovina il programma dal nome del campione (CASSA, VOX, BASS,
PIANO, LOOP...), oppure arriva dall'analisi di SPACCA, che sa benissimo cos'e'
batteria e cos'e' voce. Se sbaglia, l'ottavo rotativo nella pagina SUONO la
cambia a mano.

## Quanti tasti lavorano

Un collaudo preme tutti i tasti uno per uno, in tutte le pagine, con la macchina
appena accesa e con la macchina in mezzo al lavoro, e controlla che qualcosa
cambi davvero: non basta che compaia il nome sul display. Al momento ne
rispondono 58 piu' il TAP, che ha un collaudo suo perche' ha bisogno del tempo
vero. Quattro passano la palla alla finestra (SETUP, SFOGLIA, A LIVE, SCEGLI),
SHIFT per mestiere cambia gli altri, e resta spento solo AGG EFFETTO, che
aspetta gli effetti.

## L'onda sul display

Nelle pagine SUONO, FETTE e CAMPIONA il display fa vedere il campione del pad
scelto: la fascia centrale e' l'onda, i campi dei rotativi si stringono sotto.

- il pezzo **dentro il ritaglio** e' pieno, quello fuori resta smorto: si vede
  che c'e' ancora ma che non suona. Le due maniglie bianche con la linguetta
  sono inizio e fine, e si muovono col quarto e quinto rotativo
- nella pagina FETTE si vedono anche **i tagli che RIEMPI farebbe adesso**,
  numerati fino a sedici. Il primo rotativo cambia quante fette sono, e le
  righe si spostano mentre lo si gira: si decide guardando, prima di premere
- l'onda e' del colore della famiglia del suono, come il pad
- i picchi si calcolano una volta sola per campione, non venti volte al
  secondo: su un brano di un minuto sono due milioni e mezzo di campioni da
  scorrere. Il conto si rifa' solo quando cambia il pad scelto o il campione
  che ci sta sopra

## La pagina PASSO: lo step sequencer

Le quattro file in alto diventano 32 passi del suono scelto, da sinistra a
destra e dall'alto in basso. Le quattro file in basso restano i pad dei due
banchi di sotto, cosi' mentre si scrivono i passi si puo' ancora battere e
cambiare suono.

- premere una cella in alto mette o toglie il colpo su quel passo
- il passo scritto si accende col colore del suono, quello vuoto resta smorto,
  e la testina che passa accende la cella come se suonasse
- quanto vale un passo lo dice la colonna a destra dei pad: con 1/16 i 32 passi
  sono due battute
- se la sequenza e' piu' lunga, i tasti pagina avanti e indietro (o PASSI + e
  PASSI -) spostano la finestra di 32 passi alla volta
- CANCELLA SUONO toglie dalla sequenza tutti i colpi del suono scelto

Il controllo sta dentro la macchina, non nelle superfici: la cella premuta
viene riconosciuta li', quindi i passi si scrivono allo stesso modo dal Push,
dall'OSC e dalla finestra.
