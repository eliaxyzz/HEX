# ⬢ Hex

**Version 1.1** · C++23 · SFML 3

*[🇮🇹 Leggi in italiano](README.md)*

A complete implementation of the board game **Hex**: a presentation-independent game engine, a
**multi-threaded Monte Carlo Tree Search** AI, a polished **SFML 3** graphical interface and
**online multiplayer with an authoritative server**.

![Hex Board](https://upload.wikimedia.org/wikipedia/commons/a/a3/Hex-board-11x11-%282%29.svg)

---

## Table of contents

- [What Hex is](#what-hex-is)
- [Main features](#main-features)
  - [Game engine](#game-engine)
  - [Artificial intelligence — multi-threaded MCTS](#artificial-intelligence--multi-threaded-mcts-with-root-parallelization)
  - [Gameplay and presentation](#gameplay-and-presentation)
  - [Arcade mode](#arcade-mode)
  - [Progression, ranks and cosmetics](#progression-ranks-and-cosmetics)
  - [Tutorial: the interactive manual](#tutorial-the-interactive-manual)
  - [Saving and loading](#saving-and-loading)
  - [Online multiplayer](#online-multiplayer-clientserver)
- [Architecture](#architecture)
- [Building](#building)
- [How to play](#how-to-play)
- [How to play online](#how-to-play-online)
- [Tests](#tests)
- [Project structure](#project-structure)
- [License](#license)
- [Author](#author)

---

## What Hex is

**Hex** is an abstract strategy game for two players, played on a rhombic board of hexagonal
cells, traditionally 11×11.

- **Red** must connect the **top** side to the **bottom** one.
- **Blue** must connect the **left** side to the **right** one.

Players alternate placing a stone of their own colour on an empty cell. Stones never move and
are never removed. The first player to complete an unbroken chain between their two sides wins.

**There are no draws in Hex**: on a full board there is always exactly one winning chain. That
is a mathematical property of the game, not a convention.

### The Pie Rule

The first move in Hex gives a considerable advantage. To offset it, after the opening move the
**second player** may choose to **swap** instead of replying: the stone just played changes
colour and moves to the transposed position. It is supported natively by the engine and it is a
legal move in every respect — so the human, the Computer and the remote player all play it
under the very same rule. The tutorial devotes a chapter to it: it is the one rule of Hex that,
seen without an explanation, looks like a flaw in the game.

The same principle governs the **rematch**: at the end of a game the colours swap
automatically, because repeating the opening from the same side would turn a series of games
into a single game played over and over.

---

## Main features

### Game engine

- **Immutable state**: every move produces a new position instead of modifying the existing
  one. Replaying a game is therefore exact by construction.
- Strong types for colour, turn and outcome.
- A distinction between the ways a game can end: connection, resignation, illegal move, time
  out.
- Board of any side length: 11×11 is the default, not an assumption baked into the code.
- **No I/O and no threads** in the model: that is what makes it verifiable in full without
  opening a window or a socket.

### Artificial intelligence — multi-threaded MCTS with Root Parallelization

The search engine is a **Monte Carlo Tree Search** using the **UCB1** formula to balance
exploration and exploitation, parallelized **at the root** over `std::thread`.

- **True Root Parallelization**: the search runs on several threads
  (`std::thread::hardware_concurrency()`, capped by `MAX_SEARCH_THREADS = 16`) and each one
  grows a **completely separate** tree from the same position. When the deadline expires the
  root statistics are summed and the overall most-visited move is chosen: a move that convinces
  different trees, grown from different seeds, is more credible than one that convinces a
  single tree.
- **No locks in the search loop.** Since there is no shared tree, there is no structure the
  threads can race on: they share only read-only material (starting position, configuration,
  deadline) plus the `std::stop_token`, which is safe by construction. The price paid is
  memory: N trees instead of one.
- **Seeds independent by construction**: each tree mixes `std::random_device`, the clock and
  its own index. Relying on `random_device` alone is not enough because on some
  implementations (MinGW among them) it is deterministic, and identically seeded trees would
  repeat exactly the same simulations.
- **Cooperative cancellation preserved**: *every* thread checks the `std::stop_token` on each
  iteration, and `getMove()` waits for all workers before returning. Cancelling while the
  engine is thinking takes a few milliseconds, still returns the best move found, and leaves
  no thread alive behind it.
- Simulations run on a **Disjoint Set Union** structure with four virtual nodes for the edges:
  the win check during playouts costs amortized O(1) instead of a BFS.
- **Bridge defence** in the playouts: when the opponent invades a bridge, the simulation
  responds in the carrier cell instead of picking at random, and the value estimate becomes
  more reliable. It can be switched off for self-play comparisons.
- Three preconfigured difficulty levels, plus `MCTSConfig` to tune search time, exploration
  constant and number of trees. The two lower levels deliberately stay on **a single tree**:
  the difficulty scale is a game design choice, not a consequence of how many cores the
  player's machine has. **Hard**, on the other hand, takes everything the machine offers.

Measured with `hex_bench` on 24 cores, one second of search per position, averaged over the
three test positions:

| Trees | Playouts/s | Speed-up |
| ---: | ---: | ---: |
| 1 | 222,000 | — |
| 4 | 607,000 | 2.7× |
| 8 | 795,000 | 3.6× |
| 16 | 805,000 | 3.6× |

The gain flattens out around eight trees: past that point the bottleneck is no longer
computation but the allocator, which the threads contend for when creating nodes. That is why
there is a cap instead of simply taking every available core.

### Gameplay and presentation

Everything that follows exists for one reason: the game must *feel* alive even when nothing is
happening.

- **Streamed soundtrack.** The background track streams from disk with `sf::Music` while it
  plays, rather than going through the asset cache: a sound effect lasts less than a second and
  fits in memory, a track lasts minutes and only its decoded waveform would fit in RAM. The
  volume is adjusted from the settings across four steps, with immediate effect.
- **Dynamic background computed at runtime.** The screens without a board scroll over a
  mathematically generated hexagonal grid that advances diagonally while each cell "breathes"
  on its own. All the edges end up in a **single `sf::VertexArray`**, so hundreds of hexagons
  cost one draw call per frame.
- **Placement animations**: stones appear with an *easing* curve, and the sound starts where the
  animation starts.
- **Victory VFX**: when the game ends the winning chain is located and travelled by a glow. The
  path is computed by the same function in every context, so what lights up is exactly what the
  engine recognized as the win.
- **Interactive chapter-based tutorial**: you learn by playing, not by reading.
- **Accessibility mode (colour blindness)**: stones get a **distinctive symbol** in addition to
  the colour. Under deuteranopia and protanopia the two fills converge until they blur
  together, and the board becomes unreadable exactly when it needs to be read; a triangle and a
  diamond do not have that problem. The switch takes effect immediately, mid-game included.
- **Game clock** in the chess style: ten minutes each, and only the clock of the player to move
  runs. The time the Computer spends thinking is its own. Whoever runs out loses on time, with
  the same outcome and the same code path as a per-move timeout.
- **Italian and English**, switchable on the fly. The phrase keys are an `enum`, not strings: a
  wrong key does not compile, and a forgotten translation is caught by the test that walks the
  whole table.
- **Personal record**: wins and losses against the Computer are counted and shown in the menu,
  below the player's rank. They survive a restart along with the other preferences.

### Arcade mode

A variant turned on with a switch in the menu, changing two rules. It is not a separate mode
with its own code: it is the same engine with two general capabilities enabled.

- **Black holes.** Four cells are walled off **at random** before the first move and belong to
  neither player: they cannot be played and they connect nothing. They are a cell state
  (`Piece::BLOCKED`).
- **The draw cannot ruin the game.** There are no draws in Hex, and walled cells are the only
  thing that could break that: a wall crossing the board would cut both players off. But a
  side-to-side crossing requires **at least `size` cells**, so with a number of holes smaller
  than the side length such a cut cannot even be expressed. The function enforces the cap
  `count ≤ size - 1` on its own.
- **Blitz: ten seconds per move.** Time stops being a budget for the whole game and becomes a
  count that restarts every turn; whoever exceeds it loses instantly, human or Computer alike.
  It is a parameter of the existing clock (`ClockMode::PER_TURN`).

### Progression, ranks and cosmetics

- **Experience and levels.** A completed game against the Computer is worth **100 XP** if won
  and **25 XP** if lost (timeout included), because a lost game is a game played all the same.
  Every **500 XP** you gain a level. A game abandoned halfway is worth nothing, and not because
  of a dedicated check: experience is awarded at the point that already has the two guarantees
  needed.
- **Ranks.** The level carries a title, shown in the menu next to the XP: *Rookie*, *Hacker*,
  *Mastermind*, and from there on *Legend*.
- **Unlockable themes.** Levelling up opens two exclusive palettes, which recolour stones,
  directional edges and interface accents:

  | Style | Colours | Requirement |
  | :--- | :--- | :--- |
  | **Classic** | Red · Blue | available from the start |
  | **Toxic** | Neon green · Synthwave purple | Level 2 |
  | **Prestige** | Gold · Silver | Level 3 |

  They are **purely cosmetic** rewards, and the choice is deliberate: a progression that
  unlocks advantages turns the new player into a disadvantaged opponent.
- **The locks live in the widget, not in the drawing.** An entry not yet unlocked stays visible
  but dimmed, stating which level it requires.
- **The profile lives in its own file** (`saves/profile.ini`), separate from the preferences.
  Preferences are reversible choices; the profile is what you have earned. Someone deleting
  `settings.ini` to fix a window problem does not expect to lose their level.

### Tutorial: the interactive manual

Four chapters, of two kinds. Two are **played**: on a 5×5 board the player performs the minimal
sequence that produces a win — first as Red, then as Blue — with one cell lit at a time and a
click anywhere else simply ignored, because a tutorial that scolds teaches you to fear the
interface instead of using it. Two are **read**, on a board set up to illustrate what they say:

| Chapter | What it explains |
| :--- | :--- |
| **How to play** | Red's objective: connecting the top edge to the bottom one. |
| **Blue's turn** | The same rule rotated: left and right. It is the point a Red-only tutorial leaves uncovered — someone who learned to go down, put in charge of Blue, starts going down again. |
| **The Pie Rule** | Why the first stone changes colour on its own. It is the one rule of Hex that, seen without an explanation, looks like a flaw in the game: the screen shows the opening stone while the text explains that Blue may refuse to move and take it instead, and that it is therefore wise to open with a mediocre move. |
| **Arcade mode** | Blitz and black holes, illustrated by four cells walled off in **fixed** positions — a tutorial must show everyone the same thing. |

The two kinds are not two ways of working: an explained chapter is simply a chapter whose
script is empty. Drawing, text and navigation are the same code.

### Saving and loading

- **Multiple saves, with a name.** "Save" opens a small dialog asking what to call the game, and
  the file ends up in `saves/[name].hex`. There is no single slot to overwrite: silently losing
  the only saved game is not something the user asked for.
- **A list to manage them.** "Load game" lists the existing saves, six per page. Each row
  carries two commands next to the name: **rename** (with the field pre-filled with the current
  name) and **delete** (behind a modal confirmation). The name is the only thing distinguishing
  one game from another in a list, and a folder that grows with no way to prune it stops being
  useful after a month of games. Browsing the disk instead is not offered: the operating system
  already does that better.
- **The name is untrusted input** just as much as the content: letters, digits, space, hyphen
  and underscore are accepted, and nothing else. A name cannot contain separators or dots, so
  there is no way to write outside the folder by naming it.

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

The same game in Arcade mode adds a line and bumps the version, because without that line it
would describe a different game:

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

### Online multiplayer (client/server)

- **The server is authoritative.** The game only exists there. Clients send *intents*
  (`MOVE_INTENT`), the server passes them to the same `GameController` used by the local game
  and sends back the resulting state. A modified client cannot obtain more than what the engine
  grants: at most an `ERROR_MSG`.
- **The colour never travels.** A move is transmitted as a position alone: the colour is
  assigned by the turn, on the server.
- **Every packet is treated as hostile**: caps on lengths and counts verified *before*
  allocating, enums checked against their own range, coordinates compared against the declared
  board. A packet that fails the checks is discarded entirely.
- **Automatic resynchronization**: every update carries the complete move history and the
  client rebuilds the position from scratch. A lost packet does not leave a wrong board for the
  rest of the game.
- **Resilient to disconnections**: if a player drops, the other receives `OPPONENT_LEFT`, the
  game closes cleanly and the server immediately becomes available for a new pair. Neither
  process crashes.
- **Headless** server: no window, no assets, just network and engine. It runs on a machine
  without a graphics card or a desktop.

---

## Architecture

**Model – View – Controller** separation with one explicit constraint: the model knows neither
the view, nor who is playing, nor where the moves come from.

### The sources, by responsibility

`src/` is the only folder in the include path, and every `#include` states which layer the
header comes from: `#include "core/board.h"`. The direction of a dependency can be read without
opening the file, and a dependency pointing the wrong way stands out at a glance.

```
src/
├── core/       rules, engine, MCTS, clock, saving, translations
│               ⤷ no dependency on SFML: it is the headless-verifiable heart
├── ui/         views and widgets (console and SFML), assets, audio, icons, input
├── states/     the application screens and the state machine
├── network/    protocol and transport, shared between client and server
├── server/     the referee of hosted games: it exists only in the server process
└── client/     the entry points of the executables (GUI and console)
```

### The flow of a game

```
                 ┌─────────────────────────────────────────┐
   Model         │  move · board · game (Situation)        │  no I/O,
                 │  rules, state, winning condition        │  no threads
                 └────────────────────┬────────────────────┘
                                      │
                 ┌────────────────────┴────────────────────┐
   Controller    │  GameController                         │  non-blocking step()
                 │  turns, timeouts, history               │  notifies, doesn't print
                 └────────┬───────────────────────┬────────┘
                          │                       │
         GameObserver ◄───┘                       └──► AbstractPlayer
                          │                                │
       ┌──────────────────┴────────────┐   ┌───────────────┴────────────┐
  View │ ConsoleRenderer               │   │ HexPlayer (MCTS)           │ Player
       │ SfmlBoardRenderer             │   │ DeferredPlayer             │
       │ SfmlGameObserver              │   │  ├── SfmlHumanPlayer       │
       └───────────────────────────────┘   │  └── RemotePlayer (network)│
                                           └────────────────────────────┘
```

Four decisions hold up everything else.

**The controller advances in steps, not in a loop.** `GameController::step()` advances the game
by one turn and returns control immediately: `PLAYED`, `WAITING` (the player to move has not
decided yet) or `GAME_OVER`. A console program can call `run()`; the graphical interface and
the server call `step()` once per iteration and never give up the thread.

**Players are asynchronous.** `AbstractPlayer` exposes two overlapping contracts: the
synchronous one (`getMoveFromSit`), implemented by the bots, and the asynchronous one
(`startMove` / `tryTakeMove` / `abortMove`), used by the controller. The default adapter runs
the synchronous contract on a worker, so an existing bot works inside a GUI unchanged.

**Whoever receives a move from outside receives it under the same rules.** `DeferredPlayer`
implements the turn window and the validation once and for all; `SfmlHumanPlayer` translates a
click and `RemotePlayer` translates a packet. If clicks and network applied two different
rules, one of the two would be wrong.

**The engine does not print.** Everything that happens is notified through `GameObserver`.
Console, GUI and server are three different observers of the same engine.

---

## Building

### Requirements

| | |
| :--- | :--- |
| **Compiler** | C++23 — GCC 13+, Clang 16+, MSVC 19.38+ |
| **CMake** | 3.22 or later |
| **SFML** | 3.0.2 — downloaded and built automatically, no need to install it |
| **Other** | Thread support; OpenGL for the graphical target |

SFML is fetched via `FetchContent` and linked **statically**: there are no system dependencies
to install by hand, and in SFML 3 the audio engine (miniaudio) is compiled into the library as
well, so no SFML DLL is left to distribute.

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

On Windows with MinGW, add the generator:

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

> The first configuration downloads and builds SFML and takes a few minutes. Subsequent ones
> use the cache and are immediate.

### Executables produced

| Target | Description |
| :--- | :--- |
| **`HEX_GUI`** | The game: menu, tutorial, local game against the Computer or two-player, Arcade, online multiplayer. |
| **`HEX_Server`** | Headless server for online games. |
| **`HEX`** | Console: demonstration tournament between bots. |
| **`hex_tests`** | Test suite. |
| **`hex_bench`** | Engine performance measurement (not a test). Accepts `[ms] [repetitions] [trees]`. |

---

## How to play

For a first game it is worth going through the **Tutorial**: four chapters that show the
objective by having you reach it instead of describing it, and explain the Pie Rule and Arcade
mode.

From the main menu you choose the mode (**Human vs Computer** or **Human vs Human**), the
player names, the **variant** (Normal or Arcade) and, against the Computer, the difficulty
level and **your own colour**:

| Choice | Effect |
| :--- | :--- |
| **Red** | You open the game. |
| **Blue** | You leave the opening to the engine, which moves first. |
| **Random** | The coin is flipped when the game starts, not at selection time. |

Each player has **ten minutes** for the whole game. The clock of the player to move is marked
with `>` in the bottom bar and runs only while it is their turn; whoever runs out of time
loses.

In **Arcade** the clock changes unit: **ten seconds per move**, with the countdown shown in
tenths next to the name of the player to move, and immediate defeat for whoever exceeds it.
The board also shows four **black holes** — dimmed cells, walled off at random before the first
move, that neither player can play.

The commands are **on-screen buttons**, and the bar shows only those usable right now:

| Command | When it appears | Action |
| :--- | :--- | :--- |
| **Left click** | always | Places a stone on the cell |
| **Pie Rule** | only on the turn where it is legal | The swap rule |
| **Save** | always | Asks for a name and writes `saves/[name].hex` |
| **Resign** | during a game | Asks for confirmation, then returns to the menu |
| **New game** | after a game ends | Rematch **with swapped colours** |
| **`Esc`** | — | Resign (with confirmation) or, once the game is over, return to the menu |

A click on an occupied cell, out of turn or outside the board **has no effect at all**: the
move is validated before reaching the engine, so it is impossible to lose the game to an
illegal move. The stone preview appears only where the click would actually be accepted,
because it uses the very same rule.

The shortcuts **`S`** (Pie Rule) and **`R`** (save) remain active as accelerators, but no
command exists as a key alone.

---

## How to play online

You need a **server** and two **clients**. The server can run on the same machine as one of the
two players: a dedicated computer is not required.

### 1. The host starts the server

Double-click **`HEX_Server.exe`**. A console window opens, stays open and reports what happens:

```
[server] in ascolto sulla porta 53000
[server] scacchiera 11x11, in attesa di due giocatori (Ctrl+C per fermare)
```

The server **works in the background**: it has no interface, it should not be touched, and it
simply needs to be left open for the whole duration of the game. It is closed with `Ctrl+C` or
by closing the window.

It hosts **one game at a time**. When the game ends — by victory, by resignation or because
someone disconnects — the server frees itself and is immediately ready for two new players,
with no need to restart it.

The port and the board size can be changed from the command line:

```bash
HEX_Server.exe 53000 11      # port, board side
```

### 2. The two players connect

In **`HEX_GUI.exe`**: **Play Online** → fill in the two fields → **Connect**.

| Field | What to write |
| :--- | :--- |
| **Server address** | `127.0.0.1` if the server runs on your own machine (leaving the field empty means this). Otherwise the IP address of the host. |
| **Your name** | How you want to appear to your opponent. Empty means "Guest". |

The first of the two to connect stays **waiting for the opponent**; when the second one arrives
too, the game starts by itself on both screens. The **colour is assigned by the server**:
whoever connected first plays Red and moves first.

> To play between different machines, port **53000** must be reachable: on the same local
> network it is usually enough to allow access when the firewall asks; from the Internet you
> also need to forward the port on the host's router.

### 3. During the game

You play exactly as in a local game: click to place, **Pie Rule** when available. In addition
there is **Resign**, which concedes the game to the opponent.

The status line always says whether it is your turn or you are waiting. If the opponent closes
the game or loses the connection, the game stops and you return to the lobby with a message
explaining what happened — never a freeze, never a crash.

---

## Tests

The suite covers engine, controller, players, geometry, graphical adapters, save format,
preferences, clock, progression, Arcade mode, translations and network protocol with **over
1000 assertions**, split into **23 groups** registered individually in CTest: if something
breaks, the test name says right away which area to look at.

```bash
cmake --build build -j
cd build && ctest --output-on-failure
```

The executable can also be launched directly, in full or on a single group:

```bash
./build/hex_tests             # all groups
./build/hex_tests network     # only the network protocol
./build/hex_tests --list      # list of groups with descriptions
```

Some examples of what the suite holds down:

- The `engine` group covers the parallel search: number of trees respected, playouts that
  really do grow with the threads, correct move selection from the merged statistics,
  cancellation with eight trees returning in under half a second, and thirty repeated searches
  on eight threads.
- The `save_format` group covers the full round trip write → read → settings → rematch, the
  rejected file names (separators, directory traversal, absolute paths) and backwards
  compatibility with version 1 files.
- The `arcade` group verifies that a walled cell stays out of the legal moves **and** that it
  does not act as a bridge to an edge, and puts the MCTS to actually play on a holed board: the
  risk is not a rejection, it is a placement *inside* the hole, which the engine would propose
  if its fast copy of the board treated walled cells as free.
- The `profile` group covers levels, ranks, theme unlocking, the fallback for a choice that is
  no longer legitimate, and the full round trip of an Arcade save — including a tampered file
  claiming to play inside a black hole.
- The `clock` group covers the colour alternation in the rematch, in both modes and in both
  directions.
- A good part of the protocol checks concerns **malformed** packets: the sender is a remote
  machine, so every field is potentially hostile and rejection is the behaviour that matters
  most.

Some groups take a few seconds because they deliberately wait on real timeouts and complete
MCTS searches.

---

## Project structure

### `src/core` — rules, engine and data, without SFML

| File | Contents |
| :--- | :--- |
| `move.h` | `Piece`, `Player`, `Move`, `Action` and the `pieceOf` / `opponent` functions |
| `board.h/.cpp` | `HexBoard`: grid, adjacencies, winning condition (BFS) |
| `game.h/.cpp` | `Situation`: immutable state, legal moves, `GameStatus`, `EndReason` |
| `game_controller.h/.cpp` | `GameController`: `step()`, timeouts, history |
| `game_observer.h` | `GameObserver`: the game-event interface |
| `game_ruler.h/.cpp` | `HexGameRuler`: façade for playing a complete game |
| `player.h/.cpp` | `AbstractPlayer` (synchronous and asynchronous contracts), `HexPlayer` |
| `mcts.h/.cpp` | `MCTSPlayer`, `MCTSConfig` and `FastHexState` (DSU) |
| `deferred_player.h/.cpp` | `DeferredPlayer`: turn window and validation, shared |
| `test_players.h` | `RandomPlayer` and `SmartRandomPlayer`, baselines for the tests |
| `hex_geometry.h/.cpp` | `HexLayout`: axial coordinates, `centreOf`, `cellAt` |
| `winning_path.h/.cpp` | The winning chain, for the victory VFX |
| `game_clock.h/.cpp` | `GameClock`: two countdowns, per game or per turn (Blitz) |
| `arcade.h/.cpp` | The two Arcade rules: black hole draw and turn duration |
| `game_settings.h/.cpp` | Game settings, names by role, `rematchOf` |
| `app_preferences.h/.cpp` | Preferences and record, reading and writing `settings.ini` |
| `player_profile.h/.cpp` | Experience, levels, ranks and theme unlocking; `saves/profile.ini` |
| `save_format.h/.cpp` | Save format, parsing, validation, folder listing |
| `localization.h/.cpp` | `LocalizationManager`: the interface phrases in Italian and English |

### `src/ui` — views, widgets and resources

| File | Contents |
| :--- | :--- |
| `console_renderer.h/.cpp` | ASCII rendering and text formatting |
| `sfml_renderer.h/.cpp` | `SfmlBoardRenderer`: polygons, labels, overlays, status bar |
| `sfml_game_observer.h/.cpp` | `SfmlGameObserver`: engine events into visual state |
| `background_renderer.h/.cpp` | The animated hexagonal background, in a single vertex batch |
| `ui_widgets.h/.cpp` | Buttons, text fields and option groups, without SFML |
| `sfml_widgets.h/.cpp` | Widget drawing: nine-patch frames, flat style, text fitting |
| `ui_icons.h/.cpp` | The icons, drawn with geometric primitives |
| `sfml_theme.h` | Palette, thicknesses, colour-blind mode and the unlockable themes |
| `asset_manager.h/.cpp` | Fonts, textures and sounds: cache, fallbacks, generated placeholders |
| `audio_player.h/.cpp` | `AudioPlayer`: effect voices and streamed music |
| `sfml_human_player.h/.cpp` | `SfmlHumanPlayer`: clicks become moves |
| `sfml_input.h/.cpp` | SFML events translated into neutral `InputEvent`s |
| `window_mode.h/.cpp` | Window creation (fixed size), fullscreen and icon |
| `animation.h` · `resource_cache.h` | Easing curves and generic cache |

### `src/states` — the screens

| File | Contents |
| :--- | :--- |
| `app_state.h/.cpp` | State machine and neutral input, without SFML |
| `sfml_app_state.h` | `AppContext`: what survives screen changes |
| `main_menu_state.h/.cpp` | Menu: mode, names, difficulty, colour, start |
| `playing_state.h/.cpp` | Local game, commands, save and resign modals |
| `load_game_state.h/.cpp` | List of saves, paginated |
| `tutorial_state.h/.cpp` | Guided onboarding |
| `settings_state.h/.cpp` | Preferences |
| `network_lobby_state.h/.cpp` | Connecting to an online game |
| `network_playing_state.h/.cpp` | Online game |

### `src/network`, `src/server`, `src/client`

| File | Contents |
| :--- | :--- |
| `network/network_protocol.h/.cpp` | Opcodes, messages, defensive packet composition and reading |
| `network/network_client.h/.cpp` | `NetworkClient`: non-blocking socket and message queue |
| `network/network_match.h/.cpp` | `NetworkMatch`: the online game rebuilt client-side |
| `server/game_server.h/.cpp` | `GameServer` and `RemotePlayer`: the authoritative server |
| `client/gui_main.cpp` | The game: events, `update()`, drawing, screen changes |
| `client/main.cpp` | Console: tournament between bots |
| `server/server_main.cpp` | Headless server |

### `tests/`

| File | Contents |
| :--- | :--- |
| `test_framework.h` | Assertion macros and counters |
| `test_doubles.h` | Support players and observers |
| `test_main.cpp` | Group dispatcher |
| `test_*.cpp` | One file per group, one per area of the system (`test_arcade.cpp` and `test_profile.cpp` among the most recent) |

---

## License

Distributed under the **MIT** license. See the [`LICENSE`](LICENSE) file.

SFML is distributed under its own license (zlib/png) and is built by the build system: see the
[SFML/SFML](https://github.com/SFML/SFML) repository.

## Author

Elia Dallanoce - 2026
