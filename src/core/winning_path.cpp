/**
 * @file winning_path.cpp
 * @brief Winning chain search implementation.
 */

#include "core/winning_path.h"

#include <algorithm>
#include <queue>

namespace hexpath {

    std::vector<std::pair<int, int>> winningPath(const hex::HexBoard& board, const hex::Player winner) {
        const int n = board.getSize();
        if (n == 0) return {};

        const hex::Piece target = pieceOf(winner);
        const bool red = (winner == hex::Player::RED);

        // -1 marks an unvisited cell, otherwise the linear index of its predecessor.
        // Seed cells are their own predecessor, which terminates the backtrack.
        std::vector<int> came_from(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), -1);
        std::queue<int> frontier;

        const auto index = [n](const int r, const int c) { return r * n + c; };

        // Seed the frontier with every winner stone lying on its starting edge.
        for (int i = 0; i < n; ++i) {
            const int r = red ? 0 : i;
            const int c = red ? i : 0;
            if (board.getPieceAtPos({r, c}) != target) continue;

            came_from[static_cast<std::size_t>(index(r, c))] = index(r, c);
            frontier.push(index(r, c));
        }

        int reached = -1;

        while (!frontier.empty() && reached < 0) {
            const int current = frontier.front();
            frontier.pop();

            const int r = current / n;
            const int c = current % n;

            // Opposite edge reached: BFS guarantees this is a shortest path.
            if ((red && r == n - 1) || (!red && c == n - 1)) {
                reached = current;
                break;
            }

            for (const auto& [nr, nc] : board.getAdjacentPos({r, c})) {
                const int next = index(nr, nc);
                if (came_from[static_cast<std::size_t>(next)] != -1) continue;
                if (board.getPieceAtPos({nr, nc}) != target) continue;

                came_from[static_cast<std::size_t>(next)] = current;
                frontier.push(next);
            }
        }

        if (reached < 0) return {};

        // Walk the predecessor chain backwards, then restore edge-to-edge order.
        std::vector<std::pair<int, int>> path;
        for (int at = reached;; ) {
            path.emplace_back(at / n, at % n);

            const int previous = came_from[static_cast<std::size_t>(at)];
            if (previous == at) break;   // seed cell
            at = previous;
        }

        std::ranges::reverse(path);
        return path;
    }
}
