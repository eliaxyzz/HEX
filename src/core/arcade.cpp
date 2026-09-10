/**
 * @file arcade.cpp
 * @brief Arcade rule variants implementation.
 */

#include "core/arcade.h"

#include <algorithm>

namespace hexplay {

    std::vector<std::pair<int, int>> applyBlackHoles(hex::HexBoard& board, const int count,
                                                     std::mt19937& rng) {
        std::vector<std::pair<int, int>> chosen;

        const int size = board.getSize();
        if (size <= 0 || count <= 0) return chosen;

        // Clamp preserving the Hex theorem: below `size` holes no board-spanning
        // wall is expressible. See arcade.h.
        int wanted = std::min(count, size - 1);
        if (wanted <= 0) return chosen;

        // Reservoir-sample the empty cells instead of shuffling them: picking
        // `wanted` elements costs O(empties) without permuting the whole vector.
        std::vector<std::pair<int, int>> empties;
        empties.reserve(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
        board.forEachEmpty([&](const int r, const int c) { empties.emplace_back(r, c); });

        wanted = std::min(wanted, static_cast<int>(empties.size()));
        if (wanted <= 0) return chosen;

        std::ranges::sample(empties, std::back_inserter(chosen),
                            static_cast<std::size_t>(wanted), rng);

        for (const std::pair<int, int>& cell : chosen) board.blockCell(cell);

        return chosen;
    }
}
