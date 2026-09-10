/**
 * @file board.h
 * @brief HexBoard: game grid, move validation and win detection.
 */

#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include <utility>
#include "core/move.h"

namespace hex {

    /**
     * @brief Hex game board: stone placement, validation and win detection.
     *
     * Cells are held in a flat row-major vector rather than a vector of vectors:
     * copying a board costs one allocation instead of one per row, and scans stay
     * contiguous. getBoardView() still exposes the matrix form, rebuilt on demand.
     *
     * A disjoint-set structure with four virtual edge nodes is maintained
     * incrementally on every placement, reducing win detection to comparing two
     * roots instead of a full traversal. That is what lets Situation::next()
     * check for a win after every move at negligible cost.
     *
     * @note Pure model: no I/O dependency. Text rendering belongs to
     * ConsoleRenderer.
     */
    class HexBoard {
    private:
        /** @brief Board side length, 11 for a standard game. */
        int size;

        /** @brief Cells in row-major order: cells[r * size + c]. */
        std::vector<Piece> cells;

        /**
         * @brief Disjoint-set over the cells plus four virtual edge nodes.
         *
         * Compact encoding: a negative entry marks a root and its magnitude is the
         * set size, a non-negative entry is the parent index. Single allocation and
         * no mutation during lookup, i.e. no path compression, so checkWin is
         * genuinely const and concurrent readers cannot race.
         */
        std::vector<int> dsu;

        /** @brief Virtual node index for the top edge (Red). */
        [[nodiscard]] int redTop() const { return size * size; }

        /** @brief Virtual node index for the bottom edge (Red). */
        [[nodiscard]] int redBottom() const { return size * size + 1; }

        /** @brief Virtual node index for the left edge (Blue). */
        [[nodiscard]] int blueLeft() const { return size * size + 2; }

        /** @brief Virtual node index for the right edge (Blue). */
        [[nodiscard]] int blueRight() const { return size * size + 3; }

        /** @brief Returns the root of the set containing `i`. */
        [[nodiscard]] int findSet(int i) const;

        /** @brief Merges two sets, attaching the smaller under the larger. */
        void unite(int a, int b);

        /** @brief Links cell `idx` to same-coloured neighbours and to its edges. */
        void connect(int idx, Piece p);

        /** @brief Rebuilds the disjoint-set from scratch, O(n^2). */
        void rebuildConnectivity();

    public:
        /**
         * @brief Builds an empty n x n board.
         * @param n Board side length.
         */
        explicit HexBoard(int n);

        /**
         * @brief Builds a board from an existing layout, for saves and fixtures.
         * @param h Square, non-empty stone matrix.
         * @throw std::invalid_argument If the matrix is empty or not square.
         */
        explicit HexBoard(const std::vector<std::vector<Piece>>& h);

        /**
         * @brief Builds a zero-sized board, unusable until reassigned.
         */
        HexBoard() : size(0), dsu(4, -1) {}

        // --- Inspection ---

        /**
         * @brief Returns the board side length.
         */
        [[nodiscard]] int getSize() const { return size; }

        /**
         * @brief Returns the board state as a stone matrix.
         * @note Rebuilt on every call. Convenient for rendering, not for inner
         * loops: engine code uses getPieceAtPos() and forEachEmpty() instead.
         */
        [[nodiscard]] std::vector<std::vector<Piece>> getBoardView() const;

        /**
         * @brief Tests whether a position lies inside the board.
         * @param pos Coordinates as {row, column}.
         */
        [[nodiscard]] bool isValidPos(std::pair<int, int> pos) const;

        /**
         * @brief Returns the content of the given cell.
         * @param pos Cell coordinates.
         * @throw std::invalid_argument If the position is out of bounds.
         */
        [[nodiscard]] Piece getPieceAtPos(std::pair<int, int> pos) const;

        /**
         * @brief Collects every position holding the given piece.
         * @param p Piece to look for.
         */
        [[nodiscard]] std::vector<std::pair<int, int>> getPosByPiece(Piece p) const;

        /**
         * @brief Counts the cells holding the given piece.
         * @note Equivalent to getPosByPiece(p).size() without building the vector.
         */
        [[nodiscard]] int countPiece(Piece p) const;

        /**
         * @brief Invokes fn(row, column) on every empty cell, in row-major order.
         * @note Lets callers generate the legal moves with a single allocation for
         * the result instead of going through an intermediate vector.
         */
        template <typename Fn>
        void forEachEmpty(Fn&& fn) const {
            for (int r = 0; r < size; ++r) {
                for (int c = 0; c < size; ++c) {
                    if (cells[static_cast<std::size_t>(r) * static_cast<std::size_t>(size)
                              + static_cast<std::size_t>(c)] == Piece::EMPTY) {
                        fn(r, c);
                    }
                }
            }
        }

        /**
         * @brief Returns the in-bounds neighbours of a cell, up to six.
         * @param pos Centre position.
         */
        [[nodiscard]] std::vector<std::pair<int, int>> getAdjacentPos(std::pair<int, int> pos) const;

        // --- Mutation ---

        /**
         * @brief Places a stone and updates connectivity in amortised O(1).
         * @param p Stone to place.
         * @param pos Target coordinates.
         * @throw std::invalid_argument If the position is out of bounds or taken.
         */
        void addPiece(Piece p, std::pair<int, int> pos);

        /**
         * @brief Clears a cell and returns the stone it held.
         * @param pos Coordinates of the cell to clear.
         * @return Removed stone, or Piece::EMPTY if there was none.
         * @note A removal cannot be expressed as a union, so connectivity is
         * rebuilt in O(n^2). Rare enough not to matter.
         */
        Piece deletePiece(std::pair<int, int> pos);

        /**
         * @brief Walls off an empty cell, creating an Arcade black hole.
         *
         * The cell becomes unplayable for both players and takes part in no
         * connection. Setup-only: no move can wall a cell and walling cannot be
         * undone.
         *
         * @param pos Coordinates of the cell to wall off.
         * @throw std::invalid_argument If the position is out of bounds or the
         * cell is not empty. Walling a played stone would silently erase a move
         * from the game history.
         */
        void blockCell(std::pair<int, int> pos);

        /**
         * @brief Applies the pie rule swap.
         *
         * Recolours the stone at the given position and mirrors it across the main
         * diagonal.
         *
         * @param pos Position of the stone being swapped.
         * @throw std::invalid_argument If the position is out of bounds or empty.
         */
        void swapPiece(std::pair<int, int> pos);

        // --- Queries ---

        /**
         * @brief Tests whether the given player has connected its two edges.
         * @param player Player to check.
         * @return true if the player has won.
         * @note Amortised O(1): connectivity is maintained on each placement
         * rather than recomputed here.
         */
        [[nodiscard]] bool checkWin(Player player) const;
    };
}

#endif //BOARD_H
