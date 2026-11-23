# Suonare la macchina senza il Push 2

La macchina ha due pannelli che valgono uguale: il Push 2, e OSC. Quello che
arriva da OSC passa per gli stessi comandi del Push, con gli stessi numeri,
quindi le due versioni non possono andare fuori passo. Si possono anche usare
insieme.

## Gli indirizzi

Questi funzionano con qualunque programma OSC, da telefono, tablet o computer.

**Quello che la macchina ascolta** (di suo sulla porta 9000)

| Indirizzo | Argomenti | Cosa fa |
|---|---|---|
| `/pad` | banco 1-8, pad 1-16, velocity 0-127 | batte un pad; velocity 0 vuol dire lasciato |
| `/button` | numero del tasto, 1 o 0 | preme un tasto del pannello |
| `/encoder` | indice 1-8, passi | gira uno degli otto rotativi |
| `/tempo` | passi | tempo |
| `/swing` | passi | swing |
| `/volume` | passi | volume generale |

I numeri dei tasti sono gli stessi del Push 2, quelli di
[docs/pannello-push2.md](pannello-push2.md): 102-109 le pagine, 20-27 i tasti
funzione, 36-43 la quantizzazione, 85 play, 86 registra, 29 stop, e via cosi'.
I rotativi mandano **passi**, non valori assoluti: 1 avanti, -1 indietro, come
fa il Push.

**Quello che la macchina manda** (di suo alla porta 9001)

| Indirizzo | Argomenti | Cosa dice |
|---|---|---|
| `/led/pad` | banco, pad, stato | `off`, `empty`, `loaded`, `selected`, `playing` |
| `/led/button` | numero del tasto, 0-127 | quanto e' acceso |
| `/screen/title` | testo | la pagina aperta |
| `/screen/field` | indice 1-8, etichetta, valore | i campi sotto il titolo |

## Dove manda e dove ascolta

Il tasto OSC nella finestra apre tre caselle: indirizzo a cui rispondere, porta
di ascolto, porta di risposta. Restano scritte in
`~/Library/Application Support/Old Machine Rebirth/osc.txt`.

Con l'iPhone che fa da access point: il telefono manda all'indirizzo del Mac
sulla porta 9000, e nel tasto OSC si mette l'indirizzo del telefono come
indirizzo a cui rispondere. L'indirizzo del Mac si legge in Impostazioni di
rete, oppure da terminale con `ipconfig getifaddr en0`.

## TouchOSC Mk1 (iPhone, iPad)

In `tools/osc/old-machine-rebirth.touchosc` c'e' il banco gia' fatto per
TouchOSC Mk1, cinque pagine in verticale: PAD con la griglia 8x8 e i LED,
PANNELLO con pagine, tasti funzione, trasporto e quantizzazione, ROTATIVI coi
piu' e meno, SCHERMO coi campi del display, ALTRO con tutti gli altri tasti.

Per installarlo: si apre il file col TouchOSC Editor sul Mac e si manda al
telefono col pulsante Sync, tenendo aperta la pagina di configurazione di
TouchOSC sull'iPhone.

Poi sul telefono, nella configurazione OSC:

- **Host**: l'indirizzo del Mac (`ipconfig getifaddr en0` in Terminale)
- **Port (outgoing)**: 9000
- **Port (incoming)**: 9001

E nella macchina, col tasto OSC, si mette l'indirizzo dell'iPhone come
indirizzo a cui rispondere. Con l'iPhone che fa da access point, l'indirizzo
del telefono di solito finisce per .1 sulla stessa rete del Mac.

TouchOSC Mk1 non sa mandare argomenti in testa al messaggio, quindi usa gli
indirizzi lunghi: `/pad/3/13`, `/button/85`, `/encoder/2/1`, `/tempo/-1`. La
macchina capisce tutte e due le forme, quella corta e quella lunga.

Il file si rigenera insieme all'altro col target `OscTemplate`.

## Banco di comandi pronto per Open Stage Control

In `tools/osc/old-machine-rebirth.json` c'e' una sessione gia' fatta: cinque
pagine, SUONA con la griglia 8x8, PANNELLO con pagine, tasti funzione,
trasporto e quantizzazione, ROTATIVI, SCHERMO coi campi del display, e ALTRO
con tutti i tasti rimanenti.

Open Stage Control gira sul Mac e si apre dal telefono con il browser, quindi
sull'iPhone non si installa niente:

1. sul Mac, `open-stage-control --send 127.0.0.1:9000 --port 9001 --load tools/osc/old-machine-rebirth.json`
2. dal telefono, Safari all'indirizzo del Mac sulla porta 8080

Il file si rigenera dalla tabella dei comandi, cosi' se cambia un tasto cambia
anche il banco:

```
cmake --build build --target OscTemplate
./build/OscTemplate_artefacts/OscTemplate tools/osc/old-machine-rebirth.json
```

## Se non risponde

- il Mac e il telefono devono stare sulla stessa rete: con l'access point del
  telefono, il Mac deve essere agganciato a quella rete
- la prima volta macOS chiede se il programma puo' ricevere dalla rete: va detto
  di si', altrimenti la porta resta chiusa senza dirlo
- la riga di stato in fondo alla finestra scrive porta di ascolto e di risposta:
  se dice OSC SPENTO, la porta era gia' occupata da un altro programma
