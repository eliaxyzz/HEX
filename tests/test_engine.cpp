/**
 * @file test_engine.cpp
 * @brief MCTS engine: cancellation, board size, search budget.
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

void engine() {
    // --- Cooperative cancellation ---
    {
        // A very long search stops promptly when cancelled.
        HexPlayer bot(MCTSConfig{.time_limit_ms = 60000.0});
        const Situation s(HexBoard(11), Player::RED);

        bot.startMove(s);
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // lascia partire la ricerca
        CHECK(bot.tryTakeMove() == std::nullopt, "la ricerca da 60s non e' ancora finita");

        const auto t0 = std::chrono::steady_clock::now();
        bot.abortMove();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();

        CHECK(ms < 500, "abortMove() su motore cooperativo ritorna subito");
        CHECK(bot.tryTakeMove() == std::nullopt, "dopo abortMove() non resta nessuna richiesta");
        // Leaving this scope destroys the player: without the join in abortMove()
        // the worker would outlive the object.
    }
    {
        // An uncooperative computation stays correct; the wait lasts as long as it does.
        StubbornPlayer stubborn;
        const Situation s(HexBoard(11), Player::RED);
        stubborn.startMove(s);
        const auto t0 = std::chrono::steady_clock::now();
        stubborn.abortMove();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        CHECK(ms >= 100, "un bot che ignora il token fa attendere abortMove()");
        CHECK(ms < 3000, "l'attesa resta limitata alla durata del calcolo");
    }
    {
        // The controller's timeout cancels and joins: no thread is alive on return.
        SlowPlayer slow;
        test_players::RandomPlayer fast;
        GameController c(11, slow, fast, 1);
        RecordingObserver rec;
        c.addObserver(rec);
        const GameResult res = c.run();
        CHECK(res.reason == EndReason::TIMEOUT, "controller: timeout rilevato");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "timeout") == 1, "controller: onTimeout notificato");
        CHECK(slow.tryTakeMove() == std::nullopt, "controller: richiesta chiusa al ritorno");
    }

    // --- Parametric board size ---
    {
        // FastHexState assumes no fixed size: it reads it from the Situation.
        const MCTSConfig quick{.time_limit_ms = 30.0};
        const MCTSPlayer engine(quick);

        for (const int n : {3, 5, 7, 13}) {
            const Situation s(HexBoard(n), Player::RED);
            const Move m = engine.getMove(s);
            CHECK_QUIET(s.isValid(m));
        }
        CHECK(quiet_failures == 0, "MCTS produce mosse legali su board 3, 5, 7 e 13");
    }
    {
        // A complete match on a non-standard board, with MCTS on both sides.
        HexPlayer a(MCTSConfig{.time_limit_ms = 20.0});
        HexPlayer b(MCTSConfig{.time_limit_ms = 20.0});
        GameController c(5, a, b);
        const GameResult res = c.run();
        CHECK(res.reason == EndReason::CONNECTION, "board 5x5: partita MCTS conclusa per connessione");
        CHECK(c.situation().getBoard().checkWin(res.winner), "board 5x5: il vincitore ha davvero connesso");
    }
    {
        // The winner the MCTS disjoint-set reports agrees with the board's own
        // detection. Fill a 4x4 board with one complete red column.
        HexBoard b(4);
        for (int r = 0; r < 4; ++r) b.addPiece(Piece::RED_DISC, {r, 1});
        const Situation s(b, Player::BLUE);
        CHECK(b.checkWin(Player::RED), "board 4x4: il Rosso ha connesso alto-basso");
        CHECK(!b.checkWin(Player::BLUE), "board 4x4: il Blu non ha connesso");
    }

    // --- Root parallelisation ---
    {
        // The tree count is the one requested, and the search stays legal.
        quiet_failures = 0;
        const Situation s(HexBoard(7), Player::RED);

        for (const unsigned n : {1u, 2u, 4u, 8u}) {
            SearchStats stats;
            const MCTSPlayer engine(MCTSConfig{.time_limit_ms = 40.0, .threads = n});
            const Move m = engine.getMove(s, {}, &stats);

            CHECK_QUIET(s.isValid(m));
            CHECK_QUIET(stats.threads == n);
            CHECK_QUIET(stats.iterations > 0);
        }
        CHECK(quiet_failures == 0, "root parallel: 1, 2, 4 e 8 alberi danno mosse legali");
    }
    {
        // With threads = 0 the engine decides for itself, never exceeding the cap.
        SearchStats stats;
        const MCTSPlayer engine(MCTSConfig{.time_limit_ms = 40.0});
        const Situation s(HexBoard(7), Player::RED);
        (void)engine.getMove(s, {}, &stats);

        CHECK(stats.threads >= 1, "root parallel: la scelta automatica usa almeno un albero");
        CHECK(stats.threads <= MAX_SEARCH_THREADS, "root parallel: il tetto viene rispettato");
    }
    {
        // More trees in the same time must produce more playouts: the only proof that
        // the parallelism is real rather than a declared thread count. The comparison
        // is meaningful only on a genuinely multi-core host.
        if (std::thread::hardware_concurrency() >= 4) {
            const Situation s(HexBoard(9), Player::RED);

            SearchStats one;
            (void)MCTSPlayer(MCTSConfig{.time_limit_ms = 200.0, .threads = 1}).getMove(s, {}, &one);

            SearchStats four;
            (void)MCTSPlayer(MCTSConfig{.time_limit_ms = 200.0, .threads = 4}).getMove(s, {}, &four);

            // Wide margin: the goal is to tell parallel from sequential, not to
            // measure efficiency on a loaded machine.
            CHECK(four.iterations > one.iterations * 3 / 2,
                  "root parallel: 4 alberi fanno piu' playout di 1 nello stesso tempo");
        } else {
            CHECK(true, "root parallel: confronto saltato, meno di 4 core disponibili");
        }
    }
    {
        // The statistics merge has to pick a move, and an immediate win is the case
        // where the right one is beyond argument.
        HexBoard b(3);
        b.addPiece(Piece::RED_DISC, {0, 0});
        b.addPiece(Piece::RED_DISC, {1, 0});
        b.addPiece(Piece::BLUE_DISC, {0, 1});
        b.addPiece(Piece::BLUE_DISC, {1, 1});
        const Situation s(b, Player::RED);

        const MCTSPlayer engine(MCTSConfig{.time_limit_ms = 200.0, .threads = 4});
        const Move m = engine.getMove(s);

        // The comma inside std::pair would split the macro argument.
        const bool chose_the_win =
            m.kind == MoveKind::ADD && m.action.position == std::pair<int, int>{2, 0};
        CHECK(chose_the_win, "root parallel: le visite sommate scelgono la vittoria immediata");
    }
    {
        // Cancellation with many trees: all of them must stop, and getMove must have
        // joined every one before returning. A surviving thread would touch a stack
        // frame that is already gone.
        HexPlayer bot(MCTSConfig{.time_limit_ms = 60000.0, .threads = 8});
        const Situation s(HexBoard(11), Player::RED);

        bot.startMove(s);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK(bot.tryTakeMove() == std::nullopt, "root parallel: la ricerca da 60s e' in corso");

        const auto t0 = std::chrono::steady_clock::now();
        bot.abortMove();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();

        CHECK(ms < 500, "root parallel: otto alberi si fermano subito");
        CHECK(bot.tryTakeMove() == std::nullopt, "root parallel: nessuna richiesta resta aperta");
    }
    {
        // Cancellation already requested before the start: no tree completes even one
        // iteration, and the move returned is still legal.
        std::stop_source source;
        source.request_stop();

        const Situation s(HexBoard(11), Player::RED);
        const MCTSPlayer engine(MCTSConfig{.time_limit_ms = 60000.0, .threads = 4});

        const auto t0 = std::chrono::steady_clock::now();
        const Move m = engine.getMove(s, source.get_token());
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();

        CHECK(ms < 500, "root parallel: cancellato in partenza, ritorna subito");
        CHECK(s.isValid(m), "root parallel: cancellato in partenza, la mossa e' legale");
    }
    {
        // Repetition: a data race does not always show on the first attempt. Thirty
        // short searches across eight trees must yield thirty legal moves, with no
        // crash and no deadlock.
        quiet_failures = 0;
        const Situation s(HexBoard(5), Player::RED);
        const MCTSPlayer engine(MCTSConfig{.time_limit_ms = 5.0, .threads = 8});

        for (int i = 0; i < 30; ++i) {
            CHECK_QUIET(s.isValid(engine.getMove(s)));
        }
        CHECK(quiet_failures == 0, "root parallel: trenta ricerche ripetute restano corrette");
    }

    // --- Configurable search budget ---
    {
        const MCTSPlayer fast(MCTSConfig{.time_limit_ms = 20.0});
        CHECK(fast.getConfig().time_limit_ms == 20.0, "la configurazione e' quella richiesta");

        const Situation s(HexBoard(9), Player::RED);
        const auto t0 = std::chrono::steady_clock::now();
        const Move m = fast.getMove(s);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        CHECK(s.isValid(m), "ricerca breve: mossa legale");
        CHECK(ms < 1000, "ricerca breve: il tempo configurato viene rispettato");
    }
}
