/**
 * @file test_rules.cpp
 * @brief Rules, immutable state and match outcomes.
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

void rules() {
    // --- Basic types ---
    {
        CHECK(pieceOf(Player::RED) == Piece::RED_DISC, "pieceOf(RED) == RED_DISC");
        CHECK(opponent(Player::RED) == Player::BLUE, "opponent(RED) == BLUE");
        CHECK(statusFor(Player::BLUE) == GameStatus::BLUE_WON, "statusFor(BLUE) == BLUE_WON");
        CHECK(winnerOf(GameStatus::IN_PROGRESS) == std::nullopt, "winnerOf(IN_PROGRESS) vuoto");
    }

    // --- Initial state ---
    {
        Situation s(HexBoard(11), Player::RED);
        CHECK(!s.isOver(), "partita iniziale in corso");
        CHECK(s.toMove() == Player::RED, "muove il Rosso");
        CHECK(s.getStatus() == GameStatus::IN_PROGRESS, "status IN_PROGRESS");
        CHECK(s.getEndReason() == EndReason::NONE, "reason NONE a inizio partita");
        CHECK(s.winner() == std::nullopt, "nessun vincitore a inizio partita");
        CHECK(!s.isValid(add(Player::BLUE, {0,0})), "il Blu non puo' muovere al turno del Rosso");
        CHECK(!s.isValid(add(Player::RED, {99,99})), "posizione fuori board rifiutata senza eccezione");
    }

    // --- Regression: the pie rule ---
    {
        Situation s0(HexBoard(11), Player::RED);
        Situation s1 = s0.next(add(Player::RED, {2,5}));
        CHECK(s1.toMove() == Player::BLUE, "dopo la mossa del Rosso muove il Blu");
        CHECK(s1.isPieRuleActive(), "pie rule attiva alla prima mossa del Blu");

        Situation s2 = s1.next(Move(MoveKind::PIE, Action(ActionKind::SWAP, Piece::BLUE_DISC, {2,5})));
        CHECK(s2.toMove() == Player::RED, "dopo la Pie Rule il turno torna al Rosso");
        CHECK(!s2.isOver(), "la Pie Rule non chiude la partita");
        CHECK(!s2.isPieRuleActive(), "pie rule consumata");
        auto blues = s2.getBoard().getPosByPiece(Piece::BLUE_DISC);
        CHECK(blues.size() == 1 && blues[0] == std::make_pair(5,2), "pedina blu trasposta in (5,2)");
    }

    // --- Win by connection ---
    {
        Situation s(HexBoard(11), Player::RED);
        for (int r = 0; r < 10; ++r) {                 // Rosso costruisce la colonna 0
            s = s.next(add(Player::RED, {r, 0}));
            s = s.next(add(Player::BLUE, {r, 5}));     // Blu gioca altrove
        }
        CHECK(!s.isOver(), "partita ancora aperta prima della mossa vincente");
        s = s.next(add(Player::RED, {10, 0}));         // chiude alto-basso
        CHECK(s.getStatus() == GameStatus::RED_WON, "il Rosso vince collegando i bordi");
        CHECK(s.winner() == Player::RED, "winner() == RED");
        CHECK(s.getEndReason() == EndReason::CONNECTION, "motivo: CONNECTION");
        CHECK(s.toMove() == Player::BLUE, "to_move avanza anche sulla mossa vincente");
        CHECK(!s.isValid(add(Player::BLUE, {0,1})), "nessuna mossa valida a partita finita");
    }

    // --- Resignation ---
    {
        Situation s(HexBoard(11), Player::RED);
        Situation r = s.next(Move{});                  // Move() di default == RESIGN
        CHECK(r.getStatus() == GameStatus::BLUE_WON, "la resa del Rosso fa vincere il Blu");
        CHECK(r.getEndReason() == EndReason::RESIGN, "motivo: RESIGN");
    }

    // --- endedBy: an event outside the rules, such as a timeout ---
    {
        Situation s(HexBoard(11), Player::RED);
        Situation t = s.endedBy(Player::BLUE, EndReason::TIMEOUT);
        CHECK(t.winner() == Player::BLUE, "endedBy assegna la vittoria al Blu");
        CHECK(t.getEndReason() == EndReason::TIMEOUT, "motivo: TIMEOUT");
        CHECK(t.isOver(), "endedBy chiude la partita");
    }
}
