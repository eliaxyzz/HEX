/**
 * @file game_ruler.h
 * @brief Referee facade driving full matches through GameController.
 */

#ifndef GAME_RULER_H
#define GAME_RULER_H

#include "core/game.h"
#include "core/game_observer.h"
#include "core/player.h"

namespace hex {

    /**
     * @brief Referee for headless Hex matches.
     *
     * Thin facade over GameController: fixes the board size once and plays
     * complete matches under a per-move time budget. Turn handling, timeout and
     * win detection live in the controller; this class owns no game loop and
     * produces no output.
     */
    class HexGameRuler {
    public:
        /**
         * @brief Fixes the board side length used by every match.
         * @param n Board side length (11 for a standard game).
         */
        explicit HexGameRuler(const int n) : size(n) {}

        /**
         * @brief Plays one full match, blocking until it ends.
         * @param p1 First player, plays RED and moves first.
         * @param p2 Second player, plays BLUE.
         * @param seconds_per_move Per-move time budget in seconds.
         * @param observer Optional listener for match events.
         * @return Winner and termination reason: connection, resignation,
         * illegal move or timeout.
         * @warning On timeout the search thread is detached and may outlive this
         * call, so both players must stay alive for the rest of the process.
         * Cooperative cancellation in the search engine will remove this
         * constraint.
         */
        GameResult play(AbstractPlayer& p1, AbstractPlayer& p2, int seconds_per_move,
                        GameObserver* observer = nullptr) const;

    private:
        /** @brief Board side length shared by every match this referee runs. */
        int size;
    };
}

#endif //GAME_RULER_H
