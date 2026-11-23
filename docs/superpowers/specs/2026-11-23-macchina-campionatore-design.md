# MB-60, macchina da campionamento con Push 2 e Mac

Progetto di Mirco (MBStudio91). Documento di progetto, 23 novembre 2026.
Nome provvisorio: **MB-60**. Alternative in fondo.

## Perche'

Con l'arrivo del Push 3 il Push 2 e' diventato un ferro vecchio che pero' non ha
niente di vecchio: 64 pad RGB sensibili al tocco, 8 encoder che sentono il dito,
una striscia, i pedali e un display a colori da 960x160. Messo davanti a un Mac,
che ci mette potenza di calcolo, ingressi, uscite, microfono e casse, diventa il
pannello di una macchina da campionamento vera.

La macchina da replicare e' l'Akai MPC60, quella del manuale v2.0 che Mirco ha
indicato, nella lettura moderna che Akai stessa ne ha dato con l'MPC Sample: il
flusso di lavoro e' quello del 1988, le comodita' sono quelle di oggi.

## Obiettivi

1. Si campiona dagli ingressi del Mac, compreso il microfono interno, con soglia
   e pre-registrazione come sull'originale.
2. Si taglia, si accorda, si mette in loop e si affetta il campione, e le fette
   finiscono sui pad.
3. Si suona e si registra una sequenza coi pad, con quantizzazione, shuffle,
   note repeat e correzione passo passo.
4. Si incatenano le sequenze in un brano.
5. Si mixa e si esce dalle uscite del Mac, con il MIDI che entra ed esce dalle
   porte del Mac.
6. Tutto si fa dal Push, senza guardare lo schermo del Mac. La finestra sul Mac
   mostra le stesse cose, in grande, per chi preferisce il mouse.
7. Il progetto si salva su disco in una cartella leggibile: i campioni come file
   WAV, il resto in un file di testo. Niente formati chiusi.

## Non obiettivi

- Non e' una DAW. Non registra tracce audio e non monta brani sulla linea del
  tempo. Quando servira' registrare l'uscita in un progetto, si fara' la versione
  VST da mettere in Ableton, che e' il secondo tempo di questo lavoro.
- Non emula il suono del convertitore dell'MPC60. Una modalita' "vintage" che
  riduce banda e risoluzione si puo' aggiungere dopo, e va verificata sul manuale
  prima di dichiarare numeri.
- Niente marchi altrui. Il pannello, la finestra e l'icona portano solo
  MBSTUDIO91. Nessuna scritta AKAI, nessun MPC, nessun carattere copiato dai loro.
- Niente touchscreen: il display del Push si guarda, non si tocca.

## Anti obiettivi

Sono le cose che, se succedono, rendono il lavoro fallito anche se funziona:

- Che diventi un programma che si usa col mouse e il Push fa da contorno.
- Che serva leggere un manuale per fare il primo battito.
- Che il Push debba essere messo a mano in User mode, o che rubi la porta a
  Ableton.
- Che un campione registrato si perda perche' il programma e' andato giu'.

## Vincoli

- JUCE, C++, Mac Intel (il Mac mini di Mirco), compilazione su GitHub Actions con
  i file pronti da scaricare. Niente compilazione in locale.
- Aspetto: skill `mbstudio-hardware-ui`. Telaio nero, serigrafia bianca
  maiuscola, LED come unica fonte di colore, manopole con la scala a tacche,
  logo stampato in alto a sinistra e impresso in basso a destra.
- Il Push lavora sulla porta User e ci si mette da solo col sysex `0A 01`,
  lasciando libera la porta Live per Ableton. All'uscita torna in Live mode.
- Il display del Push si scrive via USB, 960x160, BGR565: il codice c'e' gia' e
  funziona.

## Le strade considerate

1. **App a se', come Stage.** Finestra sua, scheda audio sua, Push come pannello.
   Riusa il codice del Push gia' scritto, tenuto come libreria condivisa.
2. **Plugin VST3/AU** dentro Ableton o dentro Stage.
3. **Una modalita' dentro Stage.**

Scelta da Mirco: la **1**, col plugin come secondo tempo. La 2 da sola mette una
macchina che sequenzia e campiona dentro un ospite che gia' sequenzia, e le due
litigano. La 3 metterebbe dentro Stage il contrario di quello che Stage e' per
scelta, cioe' un programma che non registra e non crea tracce.

**Dove vive il codice, mia scelta:** nello stesso repository di Stage, come
secondo programma, cosi' i due condividono `Source/push2` senza copiarlo, che e'
esattamente il motivo per cui questo progetto esiste. Se un giorno diventa un
prodotto a se' si sposta, portandosi dietro la cartella condivisa. Precedente in
casa: `tools/push2-test` e' gia' un secondo programma nello stesso repository.

## Com'e' fatta la macchina vera, e cosa ne teniamo

Dal manuale MPC60 v2.0:

- 16 pad su due banchi, 32 suoni in memoria, fino a 34 perche' il charleston ne
  usa tre, chiuso, medio e aperto.
- 99 sequenze da 99 tracce, risoluzione 96 impulsi per quarto, la traccia e' di
  batteria se esce sul canale MIDI 16.
- Quantizzazione a 1/8, 1/16, 1/32 anche in terzine, shuffle dal dritto alla
  terzina piena, spostamento dei colpi avanti e indietro dentro la griglia.
- Campionamento a 40 kHz, 13,1 secondi in tutto, 5 per suono; soglia da 0 a 99%,
  pre registrazione di qualche millisecondo perche' l'attacco non si tagli.
- Per ogni suono: inizio e fine, attacco e dissolvenza, accordatura, rovescio,
  volume iniziale, velocita' che comanda inizio, attacco o volume, e un secondo
  suono che parte insieme o al posto suo sopra una certa velocita'.
- Quattro tasti sotto lo schermo che cambiano nome a ogni pagina, una manopola
  DATA, i tasti piu' e meno, un tastierino numerico.

Sul Mac i limiti del ferro spariscono: frequenza quella della scheda, durata
quella che sta in memoria, voci 32 stereo come l'MPC Sample di oggi. Restano
invece, perche' sono il carattere della macchina, la soglia con la pre
registrazione, lo shuffle, il note repeat e il modo di lavorare per pagine.

## Il pannello sul Push 2

**La griglia 8x8** e' quattro banchi da 16 tutti in vista, disposti a quadranti
come sull'MPC Sample: in basso a sinistra A, in basso a destra B, in alto a
sinistra C, in alto a destra D. Il tasto dei banchi passa al gruppo E F G H. Chi
replica l'MPC60 usa solo A e B, che sono i suoi 32 suoni, e si ritrova gli altri
due quadranti liberi per le fette o per i 16 livelli, che sull'originale ti
rubavano i pad.

**Gli otto tasti sopra il display** sono le pagine: CAMPIONA, FETTE, SUONO,
SEQUENZA, PASSO, BRANO, MIX, DISCO.

**Gli otto tasti sotto il display** sono i tasti funzione della pagina aperta,
quelli che sull'MPC60 erano quattro.

**La colonna a destra** sono le otto tracce della sequenza: si sceglie e si muta.

**Gli otto encoder** sono gli otto parametri della pagina, scritti sotto, e il
tocco li azzera. L'encoder master e' la manopola DATA sul campo scelto, quello
del tempo e' il tempo.

**La striscia** e' il fader dell'MPC, assegnabile a un parametro.

**I tasti fissi**: Play, Stop e Record per la sequenza, Record con Shift per
campionare, Repeat per il note repeat, Accent per il full level, Delete per
cancellare, Undo e Redo, Tap per il tempo, Master per il volume generale.

**Il tastierino numerico non c'e'**, ed e' l'unico buco vero. Quando una pagina
chiede un numero, i pad diventano tastierino per il tempo di scriverlo: e' una
soluzione da provare sul ferro prima di darla per buona.

## Il motore

- **Audio**: `AudioDeviceManager` di JUCE, qualsiasi scheda del Mac, ingressi
  scelti dall'utente, microfono interno compreso. Uscita stereo piu' le uscite in
  piu' se la scheda le ha, per mandare i suoni fuori separati come faceva
  l'originale con le sue otto uscite.
- **Voci**: 32 stereo, campioni in memoria, accordatura per rilettura a passo
  variabile con interpolazione.
- **Campionatore**: registrazione con soglia e pre registrazione, taglio inizio e
  fine, dissolvenza, rovescio, loop, affettatura automatica sui transienti o a
  divisione regolare, e le fette che cadono sui pad in ordine.
- **Sequencer**: orologio interno a 960 impulsi per quarto, piu' fine dei 96
  dell'originale cosi' il suonato non si perde; quantizzazione e shuffle pero'
  lavorano sulla griglia dell'MPC60, perche' il groove nasce da li'. Sequenze con
  battute, metro, loop, tracce, registrazione in sovraincisione, correzione passo
  passo.
- **Brano**: catena di sequenze con ripetizioni.
- **Salvataggio**: cartella del progetto con i WAV e un file di testo. Si salva da
  solo a ogni campione registrato, cosi' un programma che va giu' non porta via
  il lavoro.
- **MIDI**: entrata e uscita dalle porte del Mac, canale 16 per la batteria come
  l'originale, sync in uscita.

## Cosa resta a chi costruisce

- Il disegno preciso di ogni pagina del display, che si fa guardando il Push
  acceso, non a tavolino.
- Il modo di scrivere i numeri senza tastierino, da provare sul ferro.
- La modalita' vintage, se e quando.
- Il nome: MB-60, oppure SP-91, oppure BATTITO. Non blocca niente.

## Ordine dei lavori, per sommi capi

1. Scheletro del programma, audio e Push accesi, finestra in stile MBStudio.
2. Suoni sui pad e voci: si suona la griglia e si sente.
3. Campionamento con soglia, taglio, salvataggio.
4. Affettatura e assegnazione alle fette.
5. Sequencer: registra, quantizza, shuffle, riproduce.
6. Passo passo, brano, mix.
7. Pagine del display rifinite e modalita' vintage.
