/**
 * @file arcade.h
 * @brief Rule variants specific to Arcade mode.
 *
 * Arcade is standard Hex plus two variants applied at match setup:
 *  - **Black holes.** Four cells walled off at random before the first move.
 *    They belong to nobody, cannot be played and connect nothing.
 *  - **Blitz.** The clock becomes a ten second budget per *move* instead of per
 *    match; overrunning it loses the game.
 *
 * @note These are rules, not presentation, so they live engine-side next to the
 * clock and the settings and stay testable without a window.
 * @note Nothing else in the codebase knows Arcade exists. HexBoard only knows
 * about Piece::BLOCKED and GameClock only about ClockMode::PER_TURN; both are
 * general capabilities that the menu selection happens to switch on.
 */

#ifndef ARCADE_H
#define ARCADE_H

#include <random>
#include <utility>
#include <vector>

#include "core/board.h"

namespace hexplay {

    /** @brief Black holes opened by an Arcade match. */
    inline constexpr int ARCADE_BLACK_HOLES = 4;

    /** @brief Per-move time budget in Arcade, in seconds. */
    inline constexpr double ARCADE_TURN_SECONDS = 10.0;

    /**
     * @brief Walls off `count` randomly chosen empty cells.
     *
     * Clamps the request to `size - 1` internally rather than trusting the
     * caller: blocked cells are the only way a Hex position could end in a draw,
     * and a wall cutting the board from side to side needs at least `size`
     * cells. Below that threshold no cut is expressible and the Hex theorem
     * still holds. The clamp never binds on a standard 11x11 board with four
     * holes; it exists for small boards, where it does.
     *
     * @param board Board to mutate; must contain empty cells.
     * @param count Requested holes, clamped down on small boards.
     * @param rng Injected random source, so a seeded test can predict the exact
     * selection.
     * @return Cells actually blocked, in selection order.
     */
    std::vector<std::pair<int, int>> applyBlackHoles(hex::HexBoard& board, int count,
                                                     std::mt19937& rng);
}

#endif //ARCADE_H
