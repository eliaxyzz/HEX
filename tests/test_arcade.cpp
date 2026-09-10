/**
 * @file test_arcade.cpp
 * @brief The two Arcade rules: black holes and Blitz.
 *
 * Two checks carry the weight here, and they are the ones a defect would keep
 * invisible until a match was already under way:
 *
 *  - a walled cell is playable by nobody, neither the human clicking nor the
 *    engine enumerating moves;
 *  - a walled cell connects nothing, so it hands no half-win to whoever happens
 *    to have one on their edge.
 */

#include "core/arcade.h"

#include <algorithm>
#include <random>
#include <set>

#include "core/game.h"
#include "core/game_clock.h"
#include "core/mcts.h"
#include "test_framework.h"

using namespace hex;
using namespace hexplay;

void arcade() {

    // --- The black holes open where they were told to ---
    {
        HexBoard board(11);
        std::mt19937 rng{12345};

        const std::vector<std::pair<int, int>> holes =
            applyBlackHoles(board, ARCADE_BLACK_HOLES, rng);

        CHECK(holes.size() == 4, "buchi: ne vengono aperti esattamente quattro");

        const std::set<std::pair<int, int>> unique(holes.begin(), holes.end());
        CHECK(unique.size() == holes.size(), "buchi: nessuna cella viene murata due volte");

        quiet_failures = 0;
        for (const auto& h : holes) CHECK_QUIET(board.getPieceAtPos(h) == Piece::BLOCKED);
        CHECK(quiet_failures == 0, "buchi: le celle scelte risultano murate");

        CHECK(board.countPiece(Piece::EMPTY) == 11 * 11 - 4,
              "buchi: le celle murate escono dal conteggio delle vuote");
    }

    // --- A walled cell is not playable ---
    {
        HexBoard board(5);
        board.blockCell({2, 2});

        const Situation s(board, Player::RED);

        const Move onto_hole(MoveKind::ADD, Action(ActionKind::ADD, Piece::RED_DISC, {2, 2}));
        CHECK(!s.isValid(onto_hole), "murata: piazzarci sopra e' una mossa illegale");

        const Move elsewhere(MoveKind::ADD, Action(ActionKind::ADD, Piece::RED_DISC, {0, 0}));
        CHECK(s.isValid(elsewhere), "murata: le altre celle restano giocabili");

        // The engine enumerates from the empty cells: were a walled one to appear
        // here, MCTS would try to play it and next() would throw.
        const std::vector<Move> moves = s.validMoves();
        CHECK(moves.size() == 5 * 5 - 1, "murata: resta fuori dalle mosse legali");

        const bool listed = std::ranges::any_of(moves, [](const Move& m) {
            return m.action.position == std::pair{2, 2};
        });
        CHECK(!listed, "murata: non compare fra le mosse generate");
    }

    // --- A walled cell connects nothing ---
    {
        // Blue wins by joining left to right. The centre row is walled at one point:
        // without the guard inside connect(), that cell would be united with Blue's
        // edges and the win would be recognised halfway across.
        HexBoard board(5);
        board.blockCell({2, 0});   // sul bordo sinistro del Blu

        for (int c = 1; c < 5; ++c) board.addPiece(Piece::BLUE_DISC, {2, c});

        CHECK(!board.checkWin(Player::BLUE),
              "murata: non fa da ponte verso il bordo, la catena resta interrotta");

        // The same chain, with the free cell actually played, does win.
        HexBoard complete(5);
        for (int c = 0; c < 5; ++c) complete.addPiece(Piece::BLUE_DISC, {2, c});
        CHECK(complete.checkWin(Player::BLUE), "controprova: la catena intera vince");
    }

    // --- The engine plays on a holed board without breaking ---
    {
        // The real risk is not a refusal but a placement inside a hole: the MCTS
        // rollout keeps its own copy of the board, and were walled cells to land there
        // as free ones it would return an illegal move that Situation::next() rejects
        // by throwing.
        HexBoard board(5);
        std::mt19937 rng{999};
        const std::vector<std::pair<int, int>> holes = applyBlackHoles(board, 4, rng);

        Situation s(board, Player::RED);

        const MCTSPlayer engine(MCTSConfig{.time_limit_ms = 60.0, .threads = 1});

        const std::set<std::pair<int, int>> blocked(holes.begin(), holes.end());

        quiet_failures = 0;
        for (int i = 0; i < 6 && !s.isOver(); ++i) {
            const Move m = engine.getMove(s);

            CHECK_QUIET(s.isValid(m));
            CHECK_QUIET(!blocked.contains(m.action.position));

            s = s.next(m);   // lancia se il motore ha proposto una cella murata
        }
        CHECK(quiet_failures == 0, "motore: nessuna mossa cade dentro un buco nero");
        CHECK(s.getBoard().countPiece(Piece::BLOCKED) == static_cast<int>(holes.size()),
              "motore: i buchi neri sono ancora tutti li' a fine sequenza");
    }

    // --- The clamp that keeps the Hex theorem standing ---
    {
        // On a 3x3 board four holes would allow a wall from side to side, so the
        // function grants at most size - 1.
        HexBoard small(3);
        std::mt19937 rng{7};
        const std::vector<std::pair<int, int>> holes = applyBlackHoles(small, 4, rng);

        CHECK(holes.size() == 2, "tetto: su una 3x3 i buchi si fermano a size - 1");
        CHECK(small.countPiece(Piece::EMPTY) == 3 * 3 - 2, "tetto: e la scacchiera resta giocabile");
    }

    // --- Blitz: the count restarts on every turn change ---
    {
        GameClock blitz(ARCADE_TURN_SECONDS, ClockMode::PER_TURN);
        CHECK(blitz.mode() == ClockMode::PER_TURN, "blitz: l'orologio e' per turno");

        blitz.setRunning(Player::RED);
        blitz.tick(4.0);
        CHECK(blitz.remaining(Player::RED) == 6.0, "blitz: il tempo del Rosso scende");

        // The same player does not reload: setRunning is called every frame.
        blitz.setRunning(Player::RED);
        CHECK(blitz.remaining(Player::RED) == 6.0, "blitz: ripetere il turno non ricarica");

        blitz.setRunning(Player::BLUE);
        blitz.tick(2.0);
        CHECK(blitz.remaining(Player::BLUE) == 8.0, "blitz: il Blu parte dal budget pieno");
        CHECK(blitz.remaining(Player::RED) == 6.0, "blitz: il Rosso resta fermo mentre non muove");

        // Returning to Red restarts its count from scratch: the difference from a
        // chess clock, where the remainder would have carried over.
        blitz.setRunning(Player::RED);
        CHECK(blitz.remaining(Player::RED) == 10.0, "blitz: al ritorno il Rosso ha di nuovo 10 secondi");
    }

    // --- Blitz: overrunning ten seconds loses ---
    {
        GameClock blitz(ARCADE_TURN_SECONDS, ClockMode::PER_TURN);

        blitz.setRunning(Player::BLUE);
        blitz.tick(9.9);
        CHECK(!blitz.expiredPlayer().has_value(), "blitz: a 0.1 secondi non e' ancora scaduto");

        blitz.tick(0.2);
        CHECK(blitz.expiredPlayer() == Player::BLUE, "blitz: superati i 10 secondi, il Blu perde");
    }

    // --- The ordinary clock is unaffected ---
    {
        GameClock normal(60.0);
        CHECK(normal.mode() == ClockMode::TOTAL, "normale: il modo predefinito e' il budget di partita");

        normal.setRunning(Player::RED);
        normal.tick(10.0);
        normal.setRunning(Player::BLUE);
        normal.setRunning(Player::RED);
        CHECK(normal.remaining(Player::RED) == 50.0,
              "normale: cambiare turno non restituisce tempo a nessuno");
    }

    // --- The tenths display ---
    {
        CHECK(GameClock::formatTenths(10.0) == "10.0", "decimi: il budget pieno");
        CHECK(GameClock::formatTenths(9.94) == "9.9", "decimi: si tronca, non si arrotonda");
        CHECK(GameClock::formatTenths(0.04) == "0.0", "decimi: sotto un decimo e' zero");
        CHECK(GameClock::formatTenths(-1.0) == "0.0", "decimi: un residuo negativo vale zero");
    }
}
