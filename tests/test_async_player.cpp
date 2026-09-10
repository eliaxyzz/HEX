/**
 * @file test_async_player.cpp
 * @brief AbstractPlayer's asynchronous contract.
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

void async_player() {
    // --- The player's asynchronous contract ---
    {
        // The default adapter turns a synchronous engine into an asynchronous one:
        // startMove does not block and tryTakeMove answers nullopt while it thinks.
        test_players::RandomPlayer bot;
        const Situation s(HexBoard(11), Player::RED);

        CHECK(bot.tryTakeMove() == std::nullopt, "nessuna richiesta in corso: tryTakeMove vuoto");

        bot.startMove(s);
        std::optional<Move> m;
        for (int i = 0; i < 20000 && !m; ++i) {
            m = bot.tryTakeMove();
            if (!m) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(m.has_value(), "adattatore di default: la mossa del bot arriva");
        CHECK(s.isValid(*m), "adattatore di default: la mossa e' legale");
        CHECK(bot.tryTakeMove() == std::nullopt, "la mossa viene consegnata una volta sola");
    }
    {
        // A player that has not decided yet does not block the controller.
        DeferredPlayer human;
        test_players::RandomPlayer bot;
        GameController c(11, human, bot);
        RecordingObserver rec;
        c.addObserver(rec);

        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 50; ++i) {
            CHECK_QUIET(c.step() == StepResult::WAITING);
        }
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();

        CHECK(quiet_failures == 0, "step() ritorna WAITING finche' l'umano non decide");
        quiet_failures = 0;
        CHECK(ms < 200, "50 step() in attesa non bloccano il chiamante");
        CHECK(human.start_calls == 1, "il giocatore riceve una sola startMove per turno");
        CHECK(c.situation().getBoard().getPosByPiece(Piece::RED_DISC).empty(),
              "in attesa la scacchiera non cambia");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "turn") == 1,
              "onTurnStart notificato una sola volta per turno");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "move") == 0,
              "nessun onMove finche' la mossa non arriva");

        // The click arrives: the move enters play on the following step.
        CHECK(human.asked().toMove() == Player::RED, "startMove riceve la situazione corrente");
        human.submit(add(Player::RED, {3, 4}));
        CHECK(stepUntilSettled(c) == StepResult::PLAYED, "consegnata la mossa, step() la applica");
        CHECK(c.situation().getBoard().getPieceAtPos({3, 4}) == Piece::RED_DISC,
              "la mossa dell'umano finisce sulla scacchiera");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "move") == 1, "onMove notificato");
    }
    {
        // A mixed human/engine match runs to completion through step() alone.
        DeferredPlayer human;
        test_players::RandomPlayer bot;
        GameController c(11, human, bot);

        int guard = 0;
        while (!c.isOver() && guard++ < 500000) {
            // The stand-in event loop delivers a legal move, but only on the human's
            // turn: one delivered out of turn would sit in the queue and go stale.
            if (c.step() == StepResult::WAITING && c.situation().toMove() == Player::RED) {
                const auto moves = c.situation().validMoves();
                human.submit(moves.front());
            }
        }
        CHECK(c.isOver(), "partita mista umano/bot conclusa");
        CHECK(c.result()->reason == EndReason::CONNECTION, "conclusa per connessione");
    }
    {
        // The timeout is enforced by the controller while it waits, not by a join.
        DeferredPlayer silent;   // non consegnera' mai una mossa
        test_players::RandomPlayer bot;
        GameController c(11, silent, bot, 1);
        RecordingObserver rec;
        c.addObserver(rec);
        const GameResult res = c.run();
        CHECK(res.winner == Player::BLUE, "l'umano che non risponde perde per timeout");
        CHECK(res.reason == EndReason::TIMEOUT, "motivo: TIMEOUT");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "timeout") == 1, "onTimeout notificato");
    }
}
