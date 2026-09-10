/**
 * @file test_history.cpp
 * @brief Move history and undo.
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

void history() {
    // --- Move history and undo ---
    {
        // The history grows with the moves and the positions stay aligned.
        DeferredPlayer red, blue;
        GameController c(11, red, blue);
        RecordingObserver rec;
        c.addObserver(rec);

        CHECK(c.moveCount() == 0, "storia vuota a inizio partita");
        CHECK(c.positions().size() == 1, "la posizione iniziale e' in storia");
        CHECK(!c.canUndo(), "niente da annullare a inizio partita");
        CHECK(!c.undo(), "undo() su storia vuota non fa nulla");

        red.submit(add(Player::RED, {0, 0}));
        stepUntilSettled(c);
        blue.submit(add(Player::BLUE, {1, 1}));
        stepUntilSettled(c);

        CHECK(c.moveCount() == 2, "due mosse in storia");
        CHECK(c.positions().size() == 3, "tre posizioni: iniziale + due mosse");
        CHECK(c.moveHistory()[0].action.position == std::make_pair(0, 0), "prima mossa registrata");
        CHECK(c.moveHistory()[1].action.position == std::make_pair(1, 1), "seconda mossa registrata");
        CHECK(c.situation().toMove() == Player::RED, "dopo due mosse tocca di nuovo al Rosso");

        // Undo of the second move.
        CHECK(c.canUndo(), "c'e' qualcosa da annullare");
        CHECK(c.undo(), "undo() riesce");
        CHECK(c.moveCount() == 1, "la mossa annullata esce dalla storia");
        CHECK(c.positions().size() == 2, "anche la posizione esce dalla storia");
        CHECK(c.situation().toMove() == Player::BLUE, "il turno torna al Blu");
        CHECK(c.situation().getBoard().getPieceAtPos({1, 1}) == Piece::EMPTY, "la pedina blu sparisce");
        CHECK(c.situation().getBoard().getPieceAtPos({0, 0}) == Piece::RED_DISC, "la pedina rossa resta");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "undo") == 1, "onUndo notificato");

        // Undo back to the initial position, and no further.
        CHECK(c.undo(), "undo() della prima mossa");
        CHECK(c.moveCount() == 0, "storia di nuovo vuota");
        CHECK(c.situation().toMove() == Player::RED, "si torna al turno del Rosso");
        CHECK(c.situation().getBoard().getPosByPiece(Piece::RED_DISC).empty(), "scacchiera di nuovo vuota");
        CHECK(!c.canUndo(), "alla posizione iniziale non c'e' piu' nulla da annullare");
        CHECK(!c.undo(), "undo() oltre l'inizio non fa nulla");
    }
    {
        // Undoing the winning move puts the match back in progress.
        DeferredPlayer red, blue;
        GameController c(11, red, blue);

        for (int r = 0; r < 10; ++r) {
            red.submit(add(Player::RED, {r, 0}));
            stepUntilSettled(c);
            blue.submit(add(Player::BLUE, {r, 5}));
            stepUntilSettled(c);
        }
        red.submit(add(Player::RED, {10, 0}));   // chiude alto-basso
        stepUntilSettled(c);

        CHECK(c.isOver(), "il Rosso ha vinto");
        CHECK(c.result()->reason == EndReason::CONNECTION, "vittoria per connessione");
        CHECK(c.step() == StepResult::GAME_OVER, "a partita finita step() non gioca");

        CHECK(c.undo(), "undo della mossa vincente");
        CHECK(!c.isOver(), "la partita e' di nuovo in corso");
        CHECK(c.situation().getStatus() == GameStatus::IN_PROGRESS, "GameStatus ripristinato");
        CHECK(c.situation().getEndReason() == EndReason::NONE, "EndReason ripristinato");
        CHECK(c.result() == std::nullopt, "nessun risultato dopo l'undo");
        CHECK(c.situation().toMove() == Player::RED, "tocca di nuovo al Rosso");
        CHECK(c.situation().getBoard().getPieceAtPos({10, 0}) == Piece::EMPTY, "la mossa vincente e' sparita");

        // And play can resume from there.
        red.submit(add(Player::RED, {10, 0}));
        CHECK(stepUntilSettled(c) == StepResult::PLAYED, "si puo' rigiocare dopo l'undo");
        CHECK(c.result()->winner == Player::RED, "rigiocando, il Rosso rivince");
    }
    {
        // Undo restores the pie rule flag as well.
        DeferredPlayer red, blue;
        GameController c(11, red, blue);

        red.submit(add(Player::RED, {2, 5}));
        stepUntilSettled(c);
        CHECK(c.situation().isPieRuleActive(), "pie rule attiva alla mossa del Blu");

        blue.submit(Move(MoveKind::PIE, Action(ActionKind::SWAP, Piece::BLUE_DISC, {2, 5})));
        stepUntilSettled(c);
        CHECK(!c.situation().isPieRuleActive(), "pie rule consumata dallo swap");
        CHECK(c.situation().toMove() == Player::RED, "dopo lo swap tocca al Rosso");

        CHECK(c.undo(), "undo dello swap");
        CHECK(c.situation().isPieRuleActive(), "pie rule di nuovo disponibile");
        CHECK(c.situation().toMove() == Player::BLUE, "il turno torna al Blu");
        CHECK(c.situation().getBoard().getPieceAtPos({2, 5}) == Piece::RED_DISC, "la pedina rossa e' tornata");
    }
    {
        // A referee-declared loss can be undone too, consuming no move.
        DeferredPlayer silent;
        test_players::RandomPlayer bot;
        GameController c(11, silent, bot, 1);
        RecordingObserver rec;
        c.addObserver(rec);

        const GameResult res = c.run();
        CHECK(res.reason == EndReason::TIMEOUT, "sconfitta per timeout");
        CHECK(c.moveCount() == 0, "il timeout non aggiunge mosse alla storia");
        CHECK(c.canUndo(), "la sconfitta a tavolino e' annullabile");

        CHECK(c.undo(), "undo del timeout");
        CHECK(!c.isOver(), "la partita e' di nuovo in corso");
        CHECK(c.situation().toMove() == Player::RED, "tocca ancora a chi era andato in timeout");
        CHECK(c.moveCount() == 0, "nessuna mossa e' stata consumata dall'undo");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "undo-end") == 1,
              "onUndo notificato senza mossa annullata");
    }
    {
        // An undo while the player is still thinking closes the request in flight.
        DeferredPlayer red, blue;
        GameController c(11, red, blue);

        red.submit(add(Player::RED, {4, 4}));
        stepUntilSettled(c);
        CHECK(c.step() == StepResult::WAITING, "il Blu sta pensando");
        CHECK(blue.start_calls == 1, "al Blu e' stata chiesta la mossa");

        CHECK(c.undo(), "undo mentre una richiesta e' in volo");
        CHECK(c.situation().toMove() == Player::RED, "il turno torna al Rosso");
        red.submit(add(Player::RED, {6, 6}));
        CHECK(stepUntilSettled(c) == StepResult::PLAYED, "il turno riparte da capo");
        CHECK(red.start_calls == 2, "al Rosso viene richiesta una nuova mossa");
        CHECK(c.situation().getBoard().getPieceAtPos({6, 6}) == Piece::RED_DISC, "la nuova mossa e' applicata");
        CHECK(c.situation().getBoard().getPieceAtPos({4, 4}) == Piece::EMPTY, "quella annullata no");
    }
}
