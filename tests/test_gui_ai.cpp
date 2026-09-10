/**
 * @file test_gui_ai.cpp
 * @brief Human versus MCTS match.
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

void gui_ai() {
    // --- Human versus MCTS ---
    //
    // Reproduces the loop of the graphical build with the engine in place of the
    // second human, including the "rewind to the human's turn" undo rule.
    {
        using hexgui::HexLayout;
        using hexgui::SfmlGameObserver;
        using hexgui::SfmlHumanPlayer;

        constexpr Player HUMAN = Player::RED;

        SfmlHumanPlayer human("Umano");
        HexPlayer bot(MCTSConfig{.time_limit_ms = 100.0});
        GameController c(11, human, bot);
        const hexui::LocalizationManager loc;
            SfmlGameObserver view(loc);
        c.addObserver(view);

        const HexLayout layout = HexLayout::fit(11, 900.0f, 604.0f, 56.0f);

        auto clickAt = [&](const std::pair<int, int> cell) {
            const auto hit = layout.cellAt(layout.centreOf(cell.first, cell.second));
            return hit && human.isArmed() && human.onCellClicked(*hit);
        };
        // Rewinds until the human is to move again, undoing the engine's reply along
        // with the human's own move.
        auto undoToHuman = [&]() {
            if (!c.undo()) return false;
            while (c.canUndo() && (c.isOver() || c.situation().toMove() != HUMAN)) {
                if (!c.undo()) break;
            }
            return true;
        };

        // The human's turn: the click becomes a move.
        c.step();
        CHECK(human.isArmed(), "IA: il primo turno arma l'umano");
        CHECK(clickAt({5, 5}), "IA: click dell'umano accettato");
        stepUntilSettled(c);
        CHECK(c.moveCount() == 1, "IA: la mossa dell'umano e' stata applicata");
        CHECK(c.situation().toMove() == Player::BLUE, "IA: ora tocca al bot");

        // While the engine thinks, the loop does not block: step() answers WAITING and
        // returns at once. That property is what keeps the window at 60 FPS.
        {
            const auto t0 = std::chrono::steady_clock::now();
            int waiting = 0;
            for (int i = 0; i < 200; ++i) {
                if (c.step() == StepResult::WAITING) ++waiting;
                else break;
            }
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - t0).count();
            CHECK(waiting == 200, "IA: step() risponde WAITING mentre il bot pensa");
            CHECK(us < 200 * 1000, "IA: 200 step() in attesa costano meno di 1 ms l'uno");
            CHECK(!human.isArmed(), "IA: durante il turno del bot l'umano non e' armato");
            CHECK(c.moveCount() == 1, "IA: in attesa la partita non avanza");
        }

        // Undo while the engine is thinking: the search is cancelled and joined, and
        // the turn returns to the human. It must be quick, since cancellation is
        // cooperative.
        {
            const auto t0 = std::chrono::steady_clock::now();
            CHECK(undoToHuman(), "IA: undo durante il turno del bot riesce");
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t0).count();
            CHECK(ms < 500, "IA: l'undo interrompe la ricerca in pochi millisecondi");
            CHECK(c.moveCount() == 0, "IA: l'undo ha ritirato la mossa dell'umano");
            CHECK(c.situation().toMove() == HUMAN, "IA: il turno e' tornato all'umano");
            CHECK(c.situation().getBoard().getPosByPiece(Piece::RED_DISC).empty(),
                  "IA: la scacchiera e' tornata vuota");
        }

        // The engine really does move, and its move is legal.
        c.step();
        CHECK(clickAt({4, 4}), "IA: nuova mossa dell'umano");
        stepUntilSettled(c);
        const std::size_t before_bot = c.moveCount();
        stepUntilSettled(c);
        CHECK(c.moveCount() == before_bot + 1, "IA: il bot ha risposto");
        CHECK(c.moveHistory().back().action.piece == Piece::BLUE_DISC, "IA: ha giocato col colore giusto");
        const bool bot_swapped = c.moveHistory().back().kind == MoveKind::PIE;
        CHECK(view.lastMove().has_value() != bot_swapped,
              "IA: l'observer evidenzia la mossa del bot, tranne quando e' uno scambio");

        // Undo after the engine's reply removes two moves rather than one, so the
        // human genuinely takes back their own move instead of making the engine
        // replay.
        {
            CHECK(c.situation().toMove() == HUMAN, "IA: tocca all'umano prima dell'undo");
            const std::size_t moves_before = c.moveCount();
            CHECK(undoToHuman(), "IA: undo dopo la risposta del bot");
            CHECK(c.moveCount() == moves_before - 2, "IA: l'undo toglie sia la mossa del bot sia la propria");
            CHECK(c.situation().toMove() == HUMAN, "IA: il turno resta all'umano");
        }

        // A complete human versus engine match, played through clicks alone.
        {
            int guard = 0;
            while (!c.isOver() && guard++ < 400) {
                if (c.step() != StepResult::WAITING) continue;

                if (human.isArmed()) {
                    for (const auto& m : c.situation().validMoves()) {
                        if (m.kind == MoveKind::ADD && clickAt(m.action.position)) break;
                    }
                } else {
                    stepUntilSettled(c);   // attende che il bot finisca di pensare
                }
            }
            CHECK(c.isOver(), "IA: partita umano contro bot conclusa");
            CHECK(c.result()->reason == EndReason::CONNECTION, "IA: conclusa per connessione");
        }

        // Undo on a finished match puts it back in play and returns the turn to the
        // human.
        {
            CHECK(c.canUndo(), "IA: undo disponibile a partita finita");
            CHECK(undoToHuman(), "IA: undo a partita finita riesce");
            CHECK(!c.isOver(), "IA: la partita e' di nuovo in corso");
            CHECK(c.situation().toMove() == HUMAN, "IA: e tocca all'umano");
            CHECK(view.result() == std::nullopt, "IA: l'observer ha ritirato l'esito");
        }
    }
}
