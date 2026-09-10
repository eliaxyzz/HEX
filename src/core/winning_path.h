/**
 * @file winning_path.h
 * @brief Reconstruction of the winning chain for end-of-game highlighting.
 *
 * @note SFML-free: a plain breadth-first search over HexBoard, so it stays unit
 * testable.
 * @note HexBoard::checkWin only compares two Union-Find roots and discards the
 * path. Rebuilding it here keeps a purely presentational need out of the engine.
 */

#ifndef WINNING_PATH_H
#define WINNING_PATH_H

#include <utility>
#include <vector>

#include "core/board.h"
#include "core/move.h"

namespace hexpath {

    /**
     * @brief Computes the shortest chain connecting the given player's two sides.
     *
     * BFS seeded with every stone of the player on its starting edge, expanding
     * only through same-coloured stones. The shortest path is returned rather
     * than the whole connected component, which would render as a blob instead
     * of a chain.
     *
     * @param board Board to inspect.
     * @param winner Player whose connection is searched.
     * @return Cells ordered from the starting edge to the opposite one, empty if
     * the player has not connected its sides.
     */
    [[nodiscard]] std::vector<std::pair<int, int>> winningPath(const hex::HexBoard& board,
                                                               hex::Player winner);
}

#endif //WINNING_PATH_H
