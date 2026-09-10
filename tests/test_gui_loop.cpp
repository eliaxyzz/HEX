/**
 * @file test_gui_loop.cpp
 * @brief Graphical loop wiring, simulated.
 */

#include "ui/console_renderer.h"
#include "core/game_controller.h"
#include "core/game_ruler.h"
#include "core/hex_geometry.h"
#include "ui/sfml_game_observer.h"
#include "ui/sfml_human_player.h"
#include "core/test_players.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <set>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "test_doubles.h"
#include "core/localization.h"
#include "test_framework.h"

using namespace hex;
using namespace hextest;

void gui_loop() {
    // --- The loop wiring, simulated without a window ---
    //
    // Reproduces the exact chain gui_main.cpp runs -- pixel -> cellAt -> armed
    // player -> onCellClicked -> step() -- without opening a window. The only thing
    // left uncovered is the transport of SFML events.
    {
        using hexgui::HexLayout;
        using hexgui::Point;
        using hexgui::SfmlGameObserver;
        using hexgui::SfmlHumanPlayer;

        SfmlHumanPlayer red("Rosso");
        SfmlHumanPlayer blue("Blu");
        GameController c(11, red, blue);
        const hexui::LocalizationManager loc;
            SfmlGameObserver view(loc);
        c.addObserver(view);

        const HexLayout layout = HexLayout::fit(11, 900.0f, 760.0f, 56.0f);

        // The armed player is the one the controller asked for a move.
        auto armed = [&]() -> SfmlHumanPlayer* {
            if (red.isArmed()) return &red;
            if (blue.isArmed()) return &blue;
            return nullptr;
        };
        // A click in pixels, as the event loop would deliver it.
        auto clickAt = [&](const Point pixel) {
            const auto cell = layout.cellAt(pixel);
            if (!cell) return false;
            SfmlHumanPlayer* p = armed();
            return p && p->onCellClicked(*cell);
        };

        c.step();   // avvia il primo turno: il Rosso viene armato
        CHECK(armed() == &red, "loop: il primo turno arma il Rosso");

        // A click in the margin: no cell, no effect.
        CHECK(!clickAt({4.0f, 4.0f}), "loop: click nel margine ignorato");
        CHECK(c.moveCount() == 0, "loop: il margine non produce mosse");

        // A click at the centre of D4 becomes Red's move.
        CHECK(clickAt(layout.centreOf(3, 3)), "loop: click su cella libera accettato");
        stepUntilSettled(c);
        CHECK(c.moveCount() == 1, "loop: il click e' diventato una mossa");
        CHECK(c.situation().getBoard().getPieceAtPos({3, 3}) == Piece::RED_DISC,
              "loop: la pedina finisce nella cella cliccata");
        CHECK(view.lastMove() == std::make_pair(3, 3), "loop: l'observer evidenzia la cella giocata");

        // Blue is to move now, and the pie rule is available: it is their first turn.
        c.step();
        CHECK(armed() == &blue, "loop: il secondo turno arma il Blu");
        CHECK(blue.canSwap(), "loop: [S] disponibile al primo turno del Blu");

        // A click on an occupied cell is refused and does not end the match.
        CHECK(!clickAt(layout.centreOf(3, 3)), "loop: click su cella occupata rifiutato");
        CHECK(!c.isOver(), "loop: un click illegale non chiude la partita");

        // The swap is played through the pie move.
        CHECK(blue.onSwapRequested(), "loop: [S] accettato");
        stepUntilSettled(c);
        CHECK(c.moveCount() == 2, "loop: lo scambio e' una mossa a tutti gli effetti");
        CHECK(c.situation().getBoard().getPieceAtPos({3, 3}) == Piece::BLUE_DISC,
              "loop: dopo lo scambio la pedina e' blu");
        CHECK(c.situation().toMove() == Player::RED, "loop: dopo lo scambio tocca al Rosso");
        CHECK(view.noticeText().find("Pie Rule") != std::string::npos, "loop: l'observer segnala lo scambio");

        // GameController::undo() reverts it, and the turn restarts from scratch.
        CHECK(c.canUndo(), "loop: [U] disponibile");
        CHECK(c.undo(), "loop: [U] annulla lo scambio");
        CHECK(c.moveCount() == 1, "loop: l'undo toglie una mossa");
        CHECK(c.situation().getBoard().getPieceAtPos({3, 3}) == Piece::RED_DISC,
              "loop: la pedina rossa e' tornata");
        CHECK(!blue.isArmed(), "loop: l'undo disarma il giocatore interrogato");

        c.step();
        CHECK(armed() == &blue, "loop: dopo l'undo il Blu viene interrogato di nuovo");
        CHECK(blue.canSwap(), "loop: e la Pie Rule e' di nuovo disponibile");

        // A complete match played through pixel clicks alone.
        int guard = 0;
        while (!c.isOver() && guard++ < 1000) {
            if (c.step() != StepResult::WAITING) continue;
            bool played = false;
            for (const auto& m : c.situation().validMoves()) {
                if (m.kind != MoveKind::ADD) continue;
                if (clickAt(layout.centreOf(m.action.position.first, m.action.position.second))) {
                    played = true;
                    break;
                }
            }
            CHECK_QUIET(played);
        }
        CHECK(quiet_failures == 0, "loop: ogni click in pixel ha prodotto una mossa");
        quiet_failures = 0;
        CHECK(c.isOver(), "loop: partita completa giocata solo con click in pixel");
        CHECK(c.result()->reason == EndReason::CONNECTION, "loop: conclusa per connessione");
        CHECK(view.result()->winner == c.result()->winner, "loop: l'observer riporta lo stesso vincitore");
        CHECK(view.statusText().find("Ha vinto:") != std::string::npos, "loop: la barra di stato annuncia l'esito");

        // Once the match ends nobody is armed: clicks fall through rather than error.
        c.step();
        CHECK(armed() == nullptr, "loop: a partita finita nessun giocatore e' armato");
        CHECK(!clickAt(layout.centreOf(0, 0)), "loop: a partita finita i click sono ignorati");
        CHECK(c.canUndo(), "loop: [U] resta disponibile a partita finita");
    }
}
