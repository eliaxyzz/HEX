/**
 * @file test_gui_input.cpp
 * @brief SfmlHumanPlayer: clicks, validation, pie rule.
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
#include "test_framework.h"

using namespace hex;
using namespace hextest;

void gui_input() {
    // --- SfmlHumanPlayer ---
    {
        using hexgui::SfmlHumanPlayer;

        // Outside its turn the input is not armed and the click is dropped.
        {
            SfmlHumanPlayer human;
            CHECK(!human.isArmed(), "umano: input disarmato alla creazione");
            CHECK(!human.onCellClicked({0, 0}), "umano: click rifiutato prima di startMove");
            CHECK(!human.hasQueuedMove(), "umano: nessuna mossa in coda");
            CHECK(human.tryTakeMove() == std::nullopt, "umano: nessuna mossa da consegnare");
        }

        // Normal cycle: armed, valid click, delivered exactly once.
        {
            SfmlHumanPlayer human;
            const Situation s0(HexBoard(11), Player::RED);

            human.startMove(s0);
            CHECK(human.isArmed(), "umano: startMove arma l'input");
            CHECK(human.onCellClicked({3, 4}), "umano: click su cella libera accettato");
            CHECK(human.hasQueuedMove(), "umano: mossa in coda");

            const auto m = human.tryTakeMove();
            CHECK(m.has_value(), "umano: la mossa viene consegnata");
            CHECK(m->kind == MoveKind::ADD && m->action.position == std::make_pair(3, 4),
                  "umano: la mossa e' quella cliccata");
            CHECK(m->action.piece == Piece::RED_DISC, "umano: il colore e' quello del giocatore di turno");
            CHECK(s0.isValid(*m), "umano: la mossa consegnata e' legale");

            CHECK(!human.isArmed(), "umano: dopo la consegna l'input e' disarmato");
            CHECK(human.tryTakeMove() == std::nullopt, "umano: la mossa non viene consegnata due volte");
            CHECK(!human.onCellClicked({5, 5}), "umano: click rifiutato dopo la consegna");
        }

        // Illegal clicks are refused and never reach the engine.
        {
            HexBoard b(11);
            b.addPiece(Piece::RED_DISC, {2, 2});
            const Situation s0(b, Player::BLUE);

            SfmlHumanPlayer human;
            human.startMove(s0);
            CHECK(!human.onCellClicked({2, 2}), "umano: click su cella occupata rifiutato");
            CHECK(!human.onCellClicked({-1, 4}), "umano: click fuori board rifiutato");
            CHECK(!human.onCellClicked({11, 0}), "umano: click oltre il bordo rifiutato");
            CHECK(!human.hasQueuedMove(), "umano: nessuna mossa illegale entra in coda");
            CHECK(human.isArmed(), "umano: dopo un click rifiutato l'input resta armato");

            CHECK(human.onCellClicked({2, 3}), "umano: il click valido successivo viene accettato");
        }

        // A second click does not overwrite the one already queued.
        {
            SfmlHumanPlayer human;
            human.startMove(Situation(HexBoard(11), Player::RED));
            CHECK(human.onCellClicked({1, 1}), "umano: primo click accettato");
            CHECK(!human.onCellClicked({7, 7}), "umano: secondo click rifiutato con una mossa in coda");
            CHECK(human.tryTakeMove()->action.position == std::make_pair(1, 1),
                  "umano: viene consegnato il primo click");
        }

        // abortMove disarms and clears, which is what the controller does on an undo
        // or a timeout.
        {
            SfmlHumanPlayer human;
            human.startMove(Situation(HexBoard(11), Player::RED));
            CHECK(human.onCellClicked({4, 4}), "umano: click accettato");
            human.abortMove();
            CHECK(!human.isArmed(), "umano: abortMove disarma l'input");
            CHECK(!human.hasQueuedMove(), "umano: abortMove svuota la coda");
            CHECK(human.tryTakeMove() == std::nullopt, "umano: dopo abortMove non c'e' nulla da consegnare");
            CHECK(!human.onCellClicked({4, 4}), "umano: dopo abortMove il click e' rifiutato");
        }

        // The synchronous contract is unusable here.
        {
            SfmlHumanPlayer human;
            bool threw = false;
            try { (void)human.getMoveFromSit(Situation(HexBoard(11), Player::RED)); }
            catch (const std::logic_error&) { threw = true; }
            CHECK(threw, "umano: getMoveFromSit lancia, il contratto e' asincrono");
        }

        // The pie rule is available only on the second player's first turn.
        {
            SfmlHumanPlayer human;

            const Situation start(HexBoard(11), Player::RED);
            human.startMove(start);
            CHECK(!human.canSwap(), "pie: non disponibile al primo turno del Rosso");
            CHECK(!human.onSwapRequested(), "pie: richiesta rifiutata quando non e' disponibile");

            const Situation after_red = start.next(add(Player::RED, {2, 5}));
            human.startMove(after_red);
            CHECK(human.canSwap(), "pie: disponibile al primo turno del Blu");
            CHECK(human.onSwapRequested(), "pie: richiesta accettata");

            const auto m = human.tryTakeMove();
            CHECK(m && m->kind == MoveKind::PIE, "pie: la mossa consegnata e' uno swap");
            CHECK(after_red.isValid(*m), "pie: lo swap e' legale");
            CHECK(!human.canSwap(), "pie: consumato dopo la consegna");

            const Situation after_swap = after_red.next(*m);
            human.startMove(after_swap);
            CHECK(!human.canSwap(), "pie: non piu' disponibile dopo lo scambio");
        }

        // A complete match runs on clicks alone, and illegal clicks do not break it.
        {
            SfmlHumanPlayer red("Umano Rosso");
            SfmlHumanPlayer blue("Umano Blu");
            GameController c(11, red, blue);

            int guard = 0;
            while (!c.isOver() && guard++ < 500) {
                if (c.step() != StepResult::WAITING) continue;

                SfmlHumanPlayer& turn = (c.situation().toMove() == Player::RED) ? red : blue;

                // A click on an occupied cell must have no effect at all.
                if (c.moveCount() > 0) {
                    const auto occupied = c.moveHistory().back().action.position;
                    CHECK_QUIET(!turn.onCellClicked(occupied));
                }

                for (const auto& m : c.situation().validMoves()) {
                    if (m.kind == MoveKind::ADD && turn.onCellClicked(m.action.position)) break;
                }
            }
            CHECK(quiet_failures == 0, "umano: i click su celle occupate restano senza effetto");
            quiet_failures = 0;
            CHECK(c.isOver(), "umano: partita fra due umani conclusa solo con i click");
            CHECK(c.result()->reason == EndReason::CONNECTION,
                  "umano: conclusa per connessione, mai per mossa illegale");
        }
    }
}
