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
| SUONO | ASCOLTA | IN LIBRERIA | ANNULLA | ROVESCIO | | | |
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
| Gli otto in fila | 71-78 | Gli otto parametri scritti sul display. Nella pagina SUONO sono livello, accordatura, panorama, inizio, fine, gruppo, modo e famiglia, nello stesso ordine in cui stanno sul display |
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

## Le luci dei tasti

Ogni tasto ha un colore e uno stato, non tutti lo stesso blu. Il trasporto, in
basso a destra del Push, usa i colori di sempre:

| Tasto | Numero | Come si accende |
|---|---|---|
| PLAY | 85 | verde pieno mentre gira, smorto da fermo |
| STOP | 29 | bianco pieno quando e' fermo, smorto mentre gira |
| REC | 86 | rosso pieno mentre scrive, lampeggiante quando aspetta |
| AUTOMATE (sovraincidi) | 89 | ambra quando e' acceso |

SHIFT con REC arma il campionatore invece della sequenza, come SAMPLE RECORD
sulla macchina vera: il tasto lampeggia finche' il suono non supera la soglia,
poi resta rosso fisso mentre registra.

Il lampeggio non lo facciamo noi a colpi di messaggi: lo fa il Push. Il colore
di partenza va sul canale 0 e l'animazione sul canale che dice tipo e durata,
dal manuale di Ableton: 11-15 lampeggio (il 14 e' un quarto), 6-10 pulsazione,
1-5 una volta sola. Il secondo colore e' lo spento, cosi' il tasto fa
acceso-spento da solo. JUCE conta i canali da 1 e il manuale da 0, quindi il
canale 14 del manuale e' il 15 di JUCE.

Gli altri tasti sono bianchi, smorti quando il comando c'e' ma non e' attivo e
pieni quando lo e'. Mute, solo e cancella sono ambra, il metronomo verde.

## La griglia: sotto i suoni, sopra le sequenze

Le otto file sono divise in due per sempre:

- **le quattro di sotto sono i suoni**, due banchi da sedici: A a sinistra e B
  a destra. Il tasto dei banchi passa alla seconda coppia, C e D. In tutto 64
  pad, quanti ne ha un programma della macchina vera
- **le quattro di sopra sono le sequenze**, in qualunque pagina

Quando si carica un kit, i primi 32 suoni vanno nella prima coppia e quelli che
avanzano nella seconda: un kit da 64 si divide da solo, e si passa da una
all'altra col tasto dei banchi.

**Acceso solo dove c'e' un suono.** Un pad vuoto resta spento, cosi' si vede a
colpo d'occhio quanto e' grande il banco che si e' caricato.

## I colori dei pad

Il colore dice che strumento e', non solo che e' un suono. La batteria va dal
blu piu' cupo all'azzurro piu' chiaro salendo di tono, cosi' guardando la
griglia si legge il kit senza leggere il display.

| Famiglia | Colore |
|---|---|
| Cassa | blu notte |
| Tom | blu |
| Rullante, rim, battimani | blu acceso |
| Percussioni | azzurro |
| Piatti: charleston, crash, ride | azzurro chiaro |
| Voce | grigio chiaro |
| Giri e fette | grigio |
| Accordi | verde scuro |
| Basso | verde piu' cupo |
| Registrato | grigio bluastro |
| Senza famiglia | grigio scuro |

Lo strumento lo indovina il programma dal nome del campione, e riconosce anche
le abbreviazioni dei kit vecchi (BD, SD, HH, CRS, TOM). Se sbaglia, l'ottavo
rotativo della pagina SUONO lo cambia a mano.

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
- l'onda e' piena, del colore della famiglia del suono
- il tasto **ASCOLTA** (F1 nella pagina SUONO, F8 nelle pagine FETTE e
  CAMPIONA) fa sentire il campione del pad scelto senza scriverlo nella
  sequenza, e un segno verde corre sull'onda mentre suona: si muove il ritaglio
  e si risente, senza uscire dalla pagina
- i picchi si calcolano una volta sola per campione, non venti volte al
  secondo: su un brano di un minuto sono due milioni e mezzo di campioni da
  scorrere. Il conto si rifa' solo quando cambia il pad scelto o il campione
  che ci sta sopra

## La pagina PASSO: lo step sequencer

Le quattro file in alto diventano una griglia: **una traccia per fila, un passo
per colonna**, quattro suoni e otto passi alla volta. Le quattro file in basso
restano i pad dei due banchi di sotto, cosi' mentre si scrivono i passi si
batte e si cambia suono senza uscire.

- premere una cella mette o toglie il colpo di quella traccia su quel passo, e
  sceglie quella traccia
- **acceso solo quello che suona**, in celeste: le celle vuote restano spente,
  cosi' la griglia si legge al volo
- una **barra luminosa** corre sulle colonne a tempo e da' il senso del tempo
  che passa. Da fermi non c'e': se non scorre niente, non c'e' niente da far
  vedere
- i **tasti scena**, la colonna a destra sotto il tasto generale, fanno
  scorrere le tracce: quello in cima sale di quattro, quello in fondo scende di
  quattro, e si accendono solo quando c'e' davvero altro da vedere sopra o
  sotto. Gli altri sei continuano a scegliere il passo, come nelle altre pagine
- i tasti pagina avanti e indietro (o PASSI + e PASSI -) scorrono i passi
- quanto vale un passo lo dice la colonna a destra: con 1/16 le otto colonne
  sono mezza battuta
- CANCELLA SUONO toglie dalla sequenza tutti i colpi del suono scelto

Il display dice quali tracce e quali passi si stanno guardando.

Il controllo sta dentro la macchina, non nelle superfici: la cella premuta
viene riconosciuta li', quindi i passi si scrivono allo stesso modo dal Push,
dall'OSC e dalla finestra.
