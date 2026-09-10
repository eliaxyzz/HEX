# ⬢ Hex

**Versione 1.1** · C++23 · SFML 3

*[🇬🇧 Read in English](README_EN.md)*

Implementazione completa del gioco da tavolo **Hex**: un motore di gioco indipendente dalla
presentazione, un'intelligenza artificiale **Monte Carlo Tree Search multi-thread**,
un'interfaccia grafica curata in **SFML 3** e un **multiplayer online con server autorevole**.

![Hex Board](https://upload.wikimedia.org/wikipedia/commons/a/a3/Hex-board-11x11-%282%29.svg)

---

## Indice

- [Cos'è Hex](#cosè-hex)
- [Caratteristiche principali](#caratteristiche-principali)
  - [Motore di gioco](#motore-di-gioco)
  - [Intelligenza artificiale — MCTS multi-thread](#intelligenza-artificiale--mcts-multi-thread-con-root-parallelization)
  - [Esperienza di gioco e presentazione](#esperienza-di-gioco-e-presentazione)
  - [Modalità Arcade](#modalità-arcade)
  - [Progressione, gradi e cosmetici](#progressione-gradi-e-cosmetici)
  - [Tutorial: il manuale interattivo](#tutorial-il-manuale-interattivo)
  - [Salvataggio e caricamento](#salvataggio-e-caricamento)
  - [Multiplayer online](#multiplayer-online-clientserver)
- [Architettura](#architettura)
- [Compilazione](#compilazione)
- [Come si gioca](#come-si-gioca)
- [Come si gioca online](#come-si-gioca-online)
- [Test](#test)
- [Struttura del progetto](#struttura-del-progetto)
- [Licenza](#licenza)
- [Autore](#autore)


---

## Cos'è Hex

**Hex** è un gioco di strategia astratta per due giocatori su una scacchiera romboidale di
celle esagonali, tradizionalmente 11×11.

- Il **Rosso** deve connettere il lato **superiore** a quello **inferiore**.
- Il **Blu** deve connettere il lato **sinistro** a quello **destro**.

I giocatori si alternano piazzando una pedina del proprio colore su una cella vuota. Le
pedine non si spostano e non si rimuovono mai. Vince chi completa per primo una catena
ininterrotta fra i propri due lati.

**In Hex non esistono pareggi**: a scacchiera piena esiste sempre esattamente una catena
vincente. È una proprietà matematica del gioco, non una convenzione.

### La Pie Rule

La prima mossa in Hex dà un vantaggio notevole. Per compensarlo, dopo la mossa di apertura il
**secondo giocatore** può scegliere di **scambiare** invece di rispondere: la pedina appena
giocata cambia colore e passa alla posizione trasposta. È supportata nativamente dal motore
ed è una mossa legale a tutti gli effetti — quindi la giocano l'umano, il Computer e il
giocatore remoto, con la stessa identica regola. Il tutorial le dedica un capitolo: è l'unica
regola di Hex che, vista senza spiegazione, sembra un difetto del gioco.

Lo stesso principio governa la **rivincita**: a fine partita i colori si alternano
automaticamente, perché ripetere l'apertura dalla stessa parte trasformerebbe una serie di
partite in una sola partita ripetuta.

---

## Caratteristiche principali

### Motore di gioco

- **Stato immutabile**: ogni mossa produce una nuova posizione invece di modificare quella
  esistente. Ricostruire una partita è quindi esatto per costruzione.
- Tipi forti per colore, turno ed esito.
- Distinzione fra i modi in cui una partita può finire: connessione, resa, mossa illegale,
  tempo scaduto.
- Scacchiera di lato qualunque: 11×11 è il default, non un'assunzione del codice.
- **Nessun I/O e nessun thread** nel modello: è ciò che lo rende verificabile per intero
  senza aprire una finestra né un socket.

### Intelligenza artificiale — MCTS multi-thread con Root Parallelization

Il motore di ricerca è una **Monte Carlo Tree Search** con formula **UCB1** per bilanciare
esplorazione e sfruttamento, parallelizzata **alla radice** su `std::thread`.

- **Root Parallelization vera**: la ricerca gira su più thread
  (`std::thread::hardware_concurrency()`, limitato da `MAX_SEARCH_THREADS = 16`) e ognuno
  coltiva un albero **completamente proprio** a partire dalla stessa posizione. Alla scadenza
  le statistiche di radice vengono sommate e si sceglie la mossa più visitata
  complessivamente: una mossa che convince alberi diversi, cresciuti da semi diversi, è più
  credibile di una che ne convince uno solo.
- **Nessun lock nel ciclo di ricerca.** Non esistendo un albero condiviso, non c'è struttura
  su cui i thread possano correre: condividono solo materiale in sola lettura (posizione di
  partenza, configurazione, scadenza) più lo `std::stop_token`, sicuro per costruzione.
  Il prezzo pagato è la memoria: N alberi invece di uno.
- **Semi indipendenti per costruzione**: ogni albero mescola `std::random_device`, l'orologio
  e il proprio indice. Non ci si affida al solo `random_device` perché su alcune
  implementazioni (MinGW fra queste) è deterministico, e alberi seminati uguale ripeterebbero
  le stesse identiche simulazioni.
- **Cancellabilità cooperativa preservata**: *ogni* thread controlla lo `std::stop_token` a
  ogni iterazione, e `getMove()` attende tutti i worker prima di ritornare. Annullare mentre il
  motore pensa richiede pochi millisecondi, restituisce comunque la miglior mossa trovata e non
  lascia un solo thread vivo dietro di sé.
- Simulazioni su una struttura **Disjoint Set Union** con quattro nodi virtuali per i bordi:
  il controllo di vittoria durante i playout costa O(1) ammortizzato invece di una BFS.
- **Difesa dei ponti** nei playout: quando l'avversario invade un ponte la simulazione
  risponde nella cella portante invece di tirare a sorte, e la stima del valore diventa più
  affidabile. Disattivabile per confronti in autogioco.
- Tre livelli di difficoltà preconfigurati, più `MCTSConfig` per regolare tempo di ricerca,
  costante di esplorazione e numero di alberi. I due livelli bassi restano deliberatamente a
  **un albero solo**: la scala delle difficoltà è una scelta di gioco, non una conseguenza di
  quanti core ha la macchina di chi gioca. **Difficile** prende invece tutto quello che la
  macchina offre.

Misurato con `hex_bench` su 24 core, un secondo di ricerca per posizione, media sulle tre
posizioni di prova:

| Alberi | Playout/s | Guadagno |
| ---: | ---: | ---: |
| 1 | 222 000 | — |
| 4 | 607 000 | 2,7× |
| 8 | 795 000 | 3,6× |
| 16 | 805 000 | 3,6× |

Il guadagno si appiattisce intorno agli otto alberi: oltre quel punto il collo di bottiglia
non è più il calcolo ma l'allocatore, che i thread si contendono per creare i nodi. È il
motivo per cui esiste un tetto invece di prendere tutti i core disponibili.

### Esperienza di gioco e presentazione

Tutto quello che segue esiste per una ragione sola: la partita deve *sembrare* viva anche
quando non sta succedendo niente.

- **Colonna sonora in streaming.** Il brano di sottofondo viaggia da disco con `sf::Music`
  mentre suona, non passa dalla cache degli asset: un effetto dura meno di un secondo e sta
  in memoria, un brano dura minuti e in RAM ci starebbe solo la sua forma d'onda decodificata.
  Il volume si regola dalle impostazioni su quattro gradini, con effetto immediato.
- **Sfondo dinamico calcolato a runtime.** Le schermate senza scacchiera scorrono su una
  griglia esagonale generata matematicamente, che avanza in diagonale mentre ogni cella
  "respira" per conto proprio. Tutti i lati finiscono in un **unico `sf::VertexArray`**,
  quindi le centinaia di esagoni costano una sola chiamata di disegno per frame.
- **Animazioni di piazzamento**: le pedine compaiono con una curva di *easing* e il suono parte dove parte l'animazione.
- **VFX di vittoria**: a partita conclusa la catena vincente viene individuata e percorsa da
  un bagliore. Il percorso lo calcola la stessa funzione in ogni contesto, quindi ciò che si
  illumina è esattamente ciò che il motore ha riconosciuto come vittoria.
- **Tutorial interattivo a capitoli**: si impara giocando, non leggendo.
- **Modalità accessibilità (daltonismo)**: le pedine ricevono un **simbolo distintivo** oltre
  al colore. Su deuteranopia e protanopia i due riempimenti si avvicinano fino a confondersi,
  e la scacchiera diventa illeggibile proprio nel momento in cui va letta; un triangolo e un
  rombo non hanno questo problema. L'interruttore ha effetto immediato, in partita compresa.
- **Orologio di partita** stile scacchi: dieci minuti a testa, e scorre solo il tempo di chi
  deve muovere. Il tempo che il Computer impiega a pensare è tempo suo. Chi lo esaurisce perde per
  tempo scaduto, con lo stesso esito e lo stesso percorso del timeout per mossa.
- **Italiano e inglese**, commutabili a caldo. Le chiavi delle frasi sono un `enum`, non
  stringhe: una chiave sbagliata non compila, e una traduzione dimenticata la trova il test
  che percorre l'intera tabella.
- **Bilancio personale**: vittorie e sconfitte contro il Computer vengono contate e mostrate
  nel menu, sotto il grado del giocatore. Sopravvivono alla chiusura insieme alle altre
  preferenze.

### Modalità Arcade

Una variante che si accende con un interruttore nel menu e cambia due regole. Non è una
modalità a parte con un proprio codice: è lo stesso motore con due capacità generali accese.

- **Buchi neri.** Quattro celle vengono murate **a caso** prima della prima mossa e non
  appartengono a nessuno dei due: non si possono giocare e non collegano niente. Sono uno
  stato della cella (`Piece::BLOCKED`).
- **Il sorteggio non può rovinare la partita.** In Hex non esistono pareggi, e le celle murate
  sono le sole che potrebbero romperlo: un muro che attraversa la scacchiera taglierebbe fuori
  entrambi i giocatori. Ma un attraversamento da lato a lato richiede **almeno `size` celle**,
  quindi con un numero di buchi inferiore al lato un taglio non è nemmeno esprimibile. La
  funzione impone da sola il tetto `count ≤ size - 1`.
- **Blitz: dieci secondi a mossa.** Il tempo smette di essere un budget per l'intera partita e
  diventa un conto che riparte a ogni turno; chi lo supera perde all'istante, umano o Computer
  che sia. È un parametro dell'orologio esistente (`ClockMode::PER_TURN`).

### Progressione, gradi e cosmetici

- **Esperienza e livelli.** Una partita conclusa contro il Computer vale **100 XP** se vinta e
  **25 XP** se persa (timeout compreso), perché una partita persa è comunque una partita
  giocata. Ogni **500 XP** si sale di livello. Una partita abbandonata a metà non vale niente,
  e non per un controllo dedicato: l'esperienza si assegna nel punto che ha già le due
  garanzie necessarie.
- **Gradi.** Il livello porta un titolo, mostrato nel menu accanto agli XP: *Novellino*,
  *Hacker*, *Mastermind*, e da lì in poi *Leggenda*.
- **Temi sbloccabili.** Salendo di livello si aprono due palette esclusive, che ritingono
  pedine, bordi direzionali e accenti dell'interfaccia:

  | Stile | Colori | Requisito |
  | :--- | :--- | :--- |
  | **Classico** | Rosso · Blu | disponibile da subito |
  | **Tossico** | Verde neon · Viola synthwave | Livello 2 |
  | **Prestigio** | Oro · Argento | Livello 3 |

  Sono ricompense **puramente cosmetiche**, e la scelta è deliberata: una progressione che
  sblocca vantaggi trasforma il giocatore nuovo in un avversario svantaggiato.
- **I lucchetti sono nel widget, non nel disegno.** Una voce non ancora sbloccata resta
  visibile ma spenta, con scritto quale livello richiede.
- **Il profilo vive in un file suo** (`saves/profile.ini`), separato dalle preferenze. Le
  preferenze sono scelte reversibili; il profilo è ciò che si è guadagnato. Chi cancella
  `settings.ini` per risolvere un problema di finestra non si aspetta di perdere il livello.

### Tutorial: il manuale interattivo

Quattro capitoli, di due nature. Due si **giocano**: su una scacchiera 5×5 il giocatore compie
la sequenza minima che produce una vittoria — prima come Rosso, poi come Blu — con una cella
illuminata per volta e un click altrove semplicemente ignorato, perché un tutorial che
rimprovera insegna a temere l'interfaccia invece che a usarla. Due si **leggono**, su una
scacchiera allestita a illustrazione di ciò che dicono:

| Capitolo | Cosa spiega |
| :--- | :--- |
| **Come si gioca** | L'obiettivo del Rosso: collegare il bordo alto a quello basso. |
| **Il turno del Blu** | La stessa regola ruotata: sinistra e destra. È il punto che un tutorial sul solo Rosso lascia scoperto — chi ha imparato a scendere, messo a giocare Blu, ricomincia a scendere. |
| **La Pie Rule** | Perché la prima pedina cambia colore da sola. È l'unica regola di Hex che, vista senza spiegazione, sembra un difetto del gioco: la schermata mostra la pedina di apertura mentre il testo spiega che il Blu può rifiutarsi di muovere e prendersela, e che conviene quindi aprire con una mossa mediocre. |
| **Modalità Arcade** | Blitz e buchi neri, illustrati da quattro celle murate in posizioni **fisse** — un tutorial deve mostrare la stessa cosa a tutti. |

Le due nature non sono due modi di funzionare: un capitolo spiegato è semplicemente un
capitolo il cui copione è vuoto. Disegno, testo e navigazione sono lo stesso codice.

### Salvataggio e caricamento

- **Salvataggi multipli, con un nome.** "Salva" apre una finestrella che chiede come chiamare
  la partita, e il file finisce in `saves/[nome].hex`. Non c'è uno slot unico da
  sovrascrivere: perdere in silenzio l'unica partita salvata non è una cosa che l'utente ha
  chiesto.
- **Un elenco per gestirle.** "Carica partita" elenca i salvataggi presenti, sei per pagina.
  Ogni riga porta accanto al nome due comandi: **rinomina** (con il campo già pieno del nome
  attuale) e **cancella** (dietro una conferma modale). Il nome è l'unica cosa che distingue
  una partita dall'altra in un elenco, e una cartella che cresce senza modo di potarla smette
  di servire dopo un mese di partite. Non si sfoglia invece il disco: quello il sistema
  operativo lo fa già meglio.
- **Il nome è input non fidato** quanto il contenuto: si accettano lettere, cifre, spazio,
  trattino e sottolineatura, e nient'altro. Un nome non può contenere separatori né puntini,
  quindi non esiste modo di scrivere fuori dalla cartella indicandolo.

```
hexsave 2
size 11
mode human_vs_ai
difficulty hard
human blue
red Computer Difficile
blue Elia
moves E5 PIE C3 F7
```

La stessa partita in modalità Arcade aggiunge una riga e sale di versione, perché senza quella
riga descriverebbe una partita diversa:

```
hexsave 3
size 11
mode human_vs_ai
difficulty medium
human red
red Elia
blue Computer Medio
holes G2 H7 F10 E11
moves F6 PIE
```

### Multiplayer online (client/server)

- **Il server è autorevole.** La partita esiste solo lì. I client mandano *intenzioni*
  (`MOVE_INTENT`), il server le passa allo stesso `GameController` della partita locale e
  rimanda indietro lo stato che ne risulta. Un client modificato non può ottenere più di
  quello che il motore concede: al massimo un `ERROR_MSG`.
- **Il colore non viaggia mai.** Una mossa si trasmette come sola posizione: il colore lo
  assegna il turno, sul server.
- **Ogni pacchetto è trattato come ostile**: tetti su lunghezze e conteggi verificati *prima*
  di allocare, enum controllati contro il proprio intervallo, coordinate confrontate con la
  scacchiera dichiarata. Un pacchetto che non supera i controlli viene scartato intero.
- **Risincronizzazione automatica**: ogni aggiornamento porta la storia completa delle mosse e
  il client ricostruisce la posizione da capo. Un pacchetto perso non lascia una scacchiera
  sbagliata per il resto della partita.
- **Resistente alle disconnessioni**: se un giocatore cade, l'altro riceve `OPPONENT_LEFT`, la
  partita si chiude in modo pulito e il server torna subito disponibile per una coppia nuova.
  Nessuno dei due processi va in crash.
- Server **headless**: nessuna finestra, nessun asset, solo rete e motore. Gira su una
  macchina senza scheda video né desktop.

---

## Architettura

Separazione **Model – View – Controller** con un vincolo esplicito: il modello non conosce né
la vista, né chi gioca, né da dove arrivano le mosse.

### I sorgenti, per responsabilità

`src/` è l'unica cartella negli include path, e ogni `#include` dice da quale strato arriva
l'intestazione: `#include "core/board.h"`. La direzione di una dipendenza si legge senza
aprire il file, e una dipendenza che va nel verso sbagliato si vede a colpo d'occhio.

```
src/
├── core/       regole, motore, MCTS, orologio, salvataggio, traduzioni
│               ⤷ nessuna dipendenza da SFML: è il cuore verificabile headless
├── ui/         viste e widget (console e SFML), asset, audio, icone, input
├── states/     le schermate dell'applicazione e la macchina a stati
├── network/    protocollo e trasporto, condivisi fra client e server
├── server/     l'arbitro delle partite ospitate: esiste solo nel processo server
└── client/     i punti di ingresso degli eseguibili (GUI e console)
```

### Il flusso di una partita

```
                 ┌─────────────────────────────────────────┐
   Model         │  move · board · game (Situation)        │  nessun I/O,
                 │  regole, stato, condizione di vittoria  │  nessun thread
                 └────────────────────┬────────────────────┘
                                      │
                 ┌────────────────────┴────────────────────┐
   Controller    │  GameController                         │  step() non bloccante
                 │  turni, timeout, cronologia             │  notifica, non stampa
                 └────────┬───────────────────────┬────────┘
                          │                       │
         GameObserver ◄───┘                       └──► AbstractPlayer
                          │                                │
       ┌──────────────────┴────────────┐   ┌───────────────┴────────────┐
  View │ ConsoleRenderer               │   │ HexPlayer (MCTS)           │ Player
       │ SfmlBoardRenderer             │   │ DeferredPlayer             │
       │ SfmlGameObserver              │   │  ├── SfmlHumanPlayer       │
       └───────────────────────────────┘   │  └── RemotePlayer (rete)   │
                                           └────────────────────────────┘
```

Quattro decisioni sostengono tutto il resto.

**Il controller avanza a passi, non in un ciclo.** `GameController::step()` fa avanzare la
partita di un turno e restituisce subito il controllo: `PLAYED`, `WAITING` (il giocatore di
turno non ha ancora deciso) o `GAME_OVER`. Un programma da console può chiamare `run()`;
l'interfaccia grafica e il server chiamano `step()` una volta per giro e non cedono mai il
thread.

**I giocatori sono asincroni.** `AbstractPlayer` espone due contratti sovrapposti: quello
sincrono (`getMoveFromSit`), che implementano i bot, e quello asincrono
(`startMove` / `tryTakeMove` / `abortMove`), che usa il controller. L'adattatore di default
esegue il contratto sincrono su un worker, quindi un bot esistente funziona dentro una GUI
senza modifiche.

**Chi riceve la mossa da fuori la riceve alle stesse regole.** `DeferredPlayer` implementa la
finestra del turno e la validazione una volta sola; `SfmlHumanPlayer` traduce un click e
`RemotePlayer` traduce un pacchetto. Se click e rete applicassero due regole diverse, una
delle due sarebbe sbagliata.

**Il motore non stampa.** Tutto ciò che accade viene notificato tramite `GameObserver`.
Console, GUI e server sono tre osservatori diversi dello stesso motore.

---

## Compilazione

### Requisiti

| | |
| :--- | :--- |
| **Compilatore** | C++23 — GCC 13+, Clang 16+, MSVC 19.38+ |
| **CMake** | 3.22 o superiore |
| **SFML** | 3.0.2 — scaricata e compilata automaticamente, non serve installarla |
| **Altro** | Supporto ai thread; OpenGL per il target grafico |

SFML viene recuperata tramite `FetchContent` e collegata **staticamente**: non ci sono
dipendenze di sistema da installare a mano, e in SFML 3 anche il motore audio (miniaudio) è
compilato dentro la libreria, quindi non resta nessuna DLL di SFML da distribuire.

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Su Windows con MinGW, aggiungere il generatore:

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

> La prima configurazione scarica e compila SFML e richiede qualche minuto. Le successive
> usano la cache e sono immediate.

### Eseguibili prodotti

| Target | Descrizione |
| :--- | :--- |
| **`HEX_GUI`** | Il gioco: menu, tutorial, partita locale contro il Computer o in due, Arcade, multiplayer online. |
| **`HEX_Server`** | Server headless per le partite online. |
| **`HEX`** | Console: torneo dimostrativo fra bot. |
| **`hex_tests`** | Suite di test. |
| **`hex_bench`** | Misura delle prestazioni del motore (non è un test). Accetta `[ms] [ripetizioni] [alberi]`. |

---

## Come si gioca

Alla prima partita conviene passare dal **Tutorial**: quattro capitoli che mostrano
l'obiettivo facendolo raggiungere invece di descriverlo, e spiegano la Pie Rule e la modalità
Arcade.

Dal menu principale si scelgono la modalità (**Umano vs Computer** o **Umano vs Umano**), i
nomi dei giocatori, la **variante** (Normale o Arcade) e, contro il Computer, il livello di
difficoltà e **il proprio colore**:

| Scelta | Effetto |
| :--- | :--- |
| **Rosso** | Apri tu la partita. |
| **Blu** | Lasci l'apertura al motore, che muove per primo. |
| **Casuale** | La moneta viene tirata all'avvio della partita, non alla selezione. |

Ogni giocatore ha **dieci minuti** per l'intera partita. L'orologio di chi deve muovere è
marcato con `>` nella barra in basso e scorre solo mentre tocca a lui; chi esaurisce il tempo
perde.

In **Arcade** l'orologio cambia unità: **dieci secondi per mossa**, con il conto alla rovescia
in decimi accanto al nome di chi deve muovere, e la sconfitta immediata a chi li supera. Sulla
scacchiera compaiono inoltre quattro **buchi neri** — celle spente, murate a caso prima della
prima mossa, che nessuno dei due può giocare.

I comandi sono **bottoni a schermo**, e la barra mostra soltanto quelli utilizzabili adesso:

| Comando | Quando compare | Azione |
| :--- | :--- | :--- |
| **Clic sinistro** | sempre | Piazza una pedina sulla cella |
| **Pie Rule** | solo nel turno in cui è legale | Regola dello scambio |
| **Salva** | sempre | Chiede un nome e scrive `saves/[nome].hex` |
| **Abbandona** | a partita in corso | Chiede conferma, poi torna al menu |
| **Nuova partita** | a partita finita | Rivincita **a colori scambiati** |
| **`Esc`** | — | Abbandona (con conferma) o, a partita finita, torna al menu |

Un clic su una cella occupata, fuori turno o fuori dalla scacchiera **non ha alcun effetto**:
la mossa viene validata prima di raggiungere il motore, quindi non è possibile perdere la
partita per una mossa illegale. L'anteprima della pedina compare solo dove il clic verrebbe
davvero accettato, perché usa la stessa identica regola.

Restano attive come acceleratori le scorciatoie **`S`** (Pie Rule) e **`R`** (salva), ma
nessun comando esiste soltanto come tasto.

---

## Come si gioca online

Servono un **server** e due **client**. Il server può girare sulla stessa macchina di uno dei
due giocatori: non serve un computer dedicato.

### 1. Chi ospita avvia il server

Doppio clic su **`HEX_Server.exe`**. Si apre una finestra di console che resta aperta e scrive
cosa succede:

```
[server] in ascolto sulla porta 53000
[server] scacchiera 11x11, in attesa di due giocatori (Ctrl+C per fermare)
```

Il server **lavora in background**: non ha un'interfaccia, non va toccato, e va semplicemente
lasciato aperto per tutta la durata della partita. Si chiude con `Ctrl+C` o chiudendo la
finestra.

Ospita **una partita per volta**. Quando la partita finisce — per vittoria, per abbandono o
perché qualcuno si disconnette — il server si libera da solo ed è subito pronto per due
giocatori nuovi, senza bisogno di riavviarlo.

Porta e dimensione della scacchiera si possono cambiare da riga di comando:

```bash
HEX_Server.exe 53000 11      # porta, lato della scacchiera
```

### 2. I due giocatori si connettono

In **`HEX_GUI.exe`**: **Gioca Online** → si compilano i due campi → **Connetti**.

| Campo | Cosa scrivere |
| :--- | :--- |
| **Indirizzo del server** | `127.0.0.1` se il server gira sulla tua stessa macchina (lasciando il campo vuoto vale questo). Altrimenti l'indirizzo IP di chi ospita. |
| **Il tuo nome** | Come vuoi comparire all'avversario. Vuoto vale "Ospite". |

Il primo dei due che si connette resta in **attesa dell'avversario**; quando arriva anche il
secondo, la partita comincia da sola su entrambi gli schermi. Il **colore lo assegna il
server**: chi si è connesso per primo gioca Rosso e muove per primo.

> Per giocare fra macchine diverse serve che la porta **53000** sia raggiungibile: sulla
> stessa rete locale di solito basta consentire l'accesso quando il firewall lo chiede; da
> Internet serve inoltrare la porta sul router di chi ospita.

### 3. Durante la partita

Si gioca esattamente come in locale: clic per piazzare, **Pie Rule** quando disponibile. In
più c'è **Abbandona**, che concede la partita all'avversario.

La riga di stato dice sempre se tocca a te o se stai aspettando. Se l'avversario chiude il
gioco o perde la connessione, la partita si interrompe e torni alla lobby con un messaggio che
spiega cosa è successo — mai un blocco, mai un crash.

---

## Test

La suite copre motore, controller, giocatori, geometria, adattatori grafici, formato di
salvataggio, preferenze, orologio, progressione, modalità Arcade, traduzioni e protocollo di
rete con **oltre 1000 asserzioni**, divise in **23 gruppi** registrati singolarmente in CTest:
se qualcosa si rompe, il nome del test dice subito quale area guardare.

```bash
cmake --build build -j
cd build && ctest --output-on-failure
```

L'eseguibile si può anche lanciare direttamente, per intero o su un solo gruppo:

```bash
./build/hex_tests             # tutti i gruppi
./build/hex_tests network     # solo il protocollo di rete
./build/hex_tests --list      # elenco dei gruppi con descrizione
```

Alcuni esempi di ciò che la suite tiene fermo:

- Il gruppo `engine` copre la ricerca parallela: numero di alberi rispettato, playout che
  crescono davvero con i thread, scelta corretta della mossa dalle statistiche fuse,
  cancellazione con otto alberi che ritorna in meno di mezzo secondo, e trenta ricerche
  ripetute a otto thread.
- Il gruppo `save_format` copre il giro completo scrittura → lettura → impostazioni →
  rivincita, i nomi di file rifiutati (separatori, risalite di cartella, percorsi assoluti) e
  la retrocompatibilità con i file della versione 1.
- Il gruppo `arcade` verifica che una cella murata resti fuori dalle mosse legali **e** che non
  faccia da ponte verso un bordo, e mette l'MCTS a giocare davvero su una scacchiera bucata:
  il rischio non è un rifiuto, è un piazzamento *dentro* il buco, che il motore proporrebbe se
  la sua copia veloce della scacchiera trattasse le celle murate come libere.
- Il gruppo `profile` copre livelli, gradi, sblocco dei temi, il ripiego di una scelta non più
  legittima, e il giro completo di un salvataggio Arcade — compreso un file manomesso che
  pretende di giocare dentro un buco nero.
- Il gruppo `clock` copre l'alternanza dei colori nella rivincita, in entrambe le modalità e in
  entrambi i versi.
- Buona parte dei controlli sul protocollo riguarda i pacchetti **malformati**: il mittente è
  una macchina remota, quindi ogni campo è potenzialmente ostile e il rifiuto è il
  comportamento che conta di più.

Alcuni gruppi durano qualche secondo perché attendono deliberatamente timeout reali e ricerche
MCTS complete.

---

## Struttura del progetto

### `src/core` — regole, motore e dati, senza SFML

| File | Contenuto |
| :--- | :--- |
| `move.h` | `Piece`, `Player`, `Move`, `Action` e le funzioni `pieceOf` / `opponent` |
| `board.h/.cpp` | `HexBoard`: griglia, adiacenze, condizione di vittoria (BFS) |
| `game.h/.cpp` | `Situation`: stato immutabile, mosse legali, `GameStatus`, `EndReason` |
| `game_controller.h/.cpp` | `GameController`: `step()`, timeout, cronologia |
| `game_observer.h` | `GameObserver`: l'interfaccia degli eventi di partita |
| `game_ruler.h/.cpp` | `HexGameRuler`: facciata per giocare una partita completa |
| `player.h/.cpp` | `AbstractPlayer` (contratti sincrono e asincrono), `HexPlayer` |
| `mcts.h/.cpp` | `MCTSPlayer`, `MCTSConfig` e `FastHexState` (DSU) |
| `deferred_player.h/.cpp` | `DeferredPlayer`: finestra del turno e validazione, condivise |
| `test_players.h` | `RandomPlayer` e `SmartRandomPlayer`, riferimenti per i test |
| `hex_geometry.h/.cpp` | `HexLayout`: coordinate assiali, `centreOf`, `cellAt` |
| `winning_path.h/.cpp` | La catena vincente, per il VFX di vittoria |
| `game_clock.h/.cpp` | `GameClock`: due conti alla rovescia, per partita o per turno (Blitz) |
| `arcade.h/.cpp` | Le due regole di Arcade: sorteggio dei buchi neri e durata del turno |
| `game_settings.h/.cpp` | Impostazioni di partita, nomi per ruolo, `rematchOf` |
| `app_preferences.h/.cpp` | Preferenze e bilancio, lettura e scrittura di `settings.ini` |
| `player_profile.h/.cpp` | Esperienza, livelli, gradi e sblocco dei temi; `saves/profile.ini` |
| `save_format.h/.cpp` | Formato dei salvataggi, analisi, validazione, elenco della cartella |
| `localization.h/.cpp` | `LocalizationManager`: le frasi dell'interfaccia in italiano e inglese |

### `src/ui` — viste, widget e risorse

| File | Contenuto |
| :--- | :--- |
| `console_renderer.h/.cpp` | Rendering ASCII e formattazione testuale |
| `sfml_renderer.h/.cpp` | `SfmlBoardRenderer`: poligoni, etichette, overlay, barra di stato |
| `sfml_game_observer.h/.cpp` | `SfmlGameObserver`: eventi del motore in stato visivo |
| `background_renderer.h/.cpp` | Lo sfondo esagonale animato, in un solo lotto di vertici |
| `ui_widgets.h/.cpp` | Bottoni, campi di testo e gruppi di opzioni, senza SFML |
| `sfml_widgets.h/.cpp` | Disegno dei widget: nove riquadri, stile piatto, adattamento del testo |
| `ui_icons.h/.cpp` | Le icone, disegnate con primitive geometriche |
| `sfml_theme.h` | Palette, spessori, modalità daltonismo e i temi sbloccabili |
| `asset_manager.h/.cpp` | Font, texture e suoni: cache, ripieghi, segnaposto generati |
| `audio_player.h/.cpp` | `AudioPlayer`: voci degli effetti e musica in streaming |
| `sfml_human_player.h/.cpp` | `SfmlHumanPlayer`: i click diventano mosse |
| `sfml_input.h/.cpp` | Eventi SFML tradotti in `InputEvent` neutri |
| `window_mode.h/.cpp` | Creazione della finestra (misura fissa), schermo intero e icona |
| `animation.h` · `resource_cache.h` | Curve di easing e cache generica |

### `src/states` — le schermate

| File | Contenuto |
| :--- | :--- |
| `app_state.h/.cpp` | Macchina a stati e input neutro, senza SFML |
| `sfml_app_state.h` | `AppContext`: ciò che sopravvive ai cambi di schermata |
| `main_menu_state.h/.cpp` | Menu: modalità, nomi, difficoltà, colore, avvio |
| `playing_state.h/.cpp` | Partita locale, comandi, modali di salvataggio e abbandono |
| `load_game_state.h/.cpp` | Elenco dei salvataggi, con pagine |
| `tutorial_state.h/.cpp` | Onboarding guidato |
| `settings_state.h/.cpp` | Preferenze |
| `network_lobby_state.h/.cpp` | Connessione a una partita online |
| `network_playing_state.h/.cpp` | Partita online |

### `src/network`, `src/server`, `src/client`

| File | Contenuto |
| :--- | :--- |
| `network/network_protocol.h/.cpp` | Opcode, messaggi, composizione e lettura difensiva dei pacchetti |
| `network/network_client.h/.cpp` | `NetworkClient`: socket non bloccante e coda dei messaggi |
| `network/network_match.h/.cpp` | `NetworkMatch`: la partita online ricostruita lato client |
| `server/game_server.h/.cpp` | `GameServer` e `RemotePlayer`: il server autorevole |
| `client/gui_main.cpp` | Il gioco: eventi, `update()`, disegno, cambi di schermata |
| `client/main.cpp` | Console: torneo fra bot |
| `server/server_main.cpp` | Server headless |

### `tests/`

| File | Contenuto |
| :--- | :--- |
| `test_framework.h` | Macro di asserzione e contatori |
| `test_doubles.h` | Giocatori e observer di supporto |
| `test_main.cpp` | Dispatcher dei gruppi |
| `test_*.cpp` | Un file per gruppo, uno per area del sistema (`test_arcade.cpp` e `test_profile.cpp` fra gli ultimi arrivati) |

---

## Licenza

Distribuito con licenza **MIT**. Vedi il file [`LICENSE`](LICENSE).

SFML è distribuita con la propria licenza (zlib/png) e viene compilata dal build system: vedi
il repository [SFML/SFML](https://github.com/SFML/SFML).

## Autore

Elia Dallanoce - 2026