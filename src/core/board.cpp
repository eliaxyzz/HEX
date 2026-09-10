/**
 * @file board.cpp
 * @brief HexBoard implementation: incremental connectivity and board mutation.
 */

#include "core/board.h"
#include <stdexcept>

namespace hex {

    namespace {
        /** @brief Axial offsets of the six neighbours of a cell. */
        constexpr int NEIGHBOUR_DR[] = {-1, -1, 0, 0, 1, 1};
        constexpr int NEIGHBOUR_DC[] = { 0,  1, -1, 1, -1, 0};
    }

    /**
     * @brief Builds an empty n x n board with a fresh disjoint-set.
     */
    HexBoard::HexBoard(const int n)
        : size(n),
          cells(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), Piece::EMPTY),
          dsu(static_cast<std::size_t>(n) * static_cast<std::size_t>(n) + 4, -1) {}

    /**
     * @brief Adopts an existing layout after checking it is square and non-empty.
     */
    HexBoard::HexBoard(const std::vector<std::vector<Piece>>& h) {
        size = static_cast<int>(h.size());
        if (size == 0 || static_cast<int>(h[0].size()) != size)
            throw std::invalid_argument("Board must be square");

        cells.resize(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
        for (int r = 0; r < size; ++r) {
            if (static_cast<int>(h[static_cast<std::size_t>(r)].size()) != size)
                throw std::invalid_argument("Board must be square");
            for (int c = 0; c < size; ++c) {
                cells[static_cast<std::size_t>(r * size + c)] =
                    h[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
            }
        }
        rebuildConnectivity();
    }

    // --- Disjoint-set --------------------------------------------------------

    int HexBoard::findSet(int i) const {
        while (dsu[static_cast<std::size_t>(i)] >= 0) i = dsu[static_cast<std::size_t>(i)];
        return i;
    }

    void HexBoard::unite(int a, int b) {
        a = findSet(a);
        b = findSet(b);
        if (a == b) return;

        // Union by size: the more negative entry is the larger set, so it wins.
        if (dsu[static_cast<std::size_t>(a)] > dsu[static_cast<std::size_t>(b)]) std::swap(a, b);
        dsu[static_cast<std::size_t>(a)] += dsu[static_cast<std::size_t>(b)];
        dsu[static_cast<std::size_t>(b)] = a;
    }

    void HexBoard::connect(const int idx, const Piece p) {
        // Only stones connect. Without this guard a blocked cell would fall into
        // the else branch below and be united with the Blue edges, handing Blue
        // half a connection for every black hole on the first column.
        if (p != Piece::RED_DISC && p != Piece::BLUE_DISC) return;

        const int r = idx / size;
        const int c = idx % size;

        // Virtual edges: Red connects top to bottom, Blue left to right.
        if (p == Piece::RED_DISC) {
            if (r == 0) unite(idx, redTop());
            if (r == size - 1) unite(idx, redBottom());
        } else {
            if (c == 0) unite(idx, blueLeft());
            if (c == size - 1) unite(idx, blueRight());
        }

        // Same-coloured neighbours.
        for (int k = 0; k < 6; ++k) {
            const int nr = r + NEIGHBOUR_DR[k];
            const int nc = c + NEIGHBOUR_DC[k];
            if (nr < 0 || nc < 0 || nr >= size || nc >= size) continue;

            const int n_idx = nr * size + nc;
            if (cells[static_cast<std::size_t>(n_idx)] == p) unite(idx, n_idx);
        }
    }

    void HexBoard::rebuildConnectivity() {
        dsu.assign(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) + 4, -1);
        for (int idx = 0; idx < size * size; ++idx) {
            if (const Piece p = cells[static_cast<std::size_t>(idx)]; p != Piece::EMPTY) {
                connect(idx, p);
            }
        }
    }

    // --- Inspection ----------------------------------------------------------

    std::vector<std::vector<Piece>> HexBoard::getBoardView() const {
        std::vector<std::vector<Piece>> view(static_cast<std::size_t>(size));
        for (int r = 0; r < size; ++r) {
            const auto first = cells.begin() + static_cast<std::ptrdiff_t>(r) * size;
            view[static_cast<std::size_t>(r)].assign(first, first + size);
        }
        return view;
    }

    /**
     * @brief Bounds check on the board coordinates.
     */
    bool HexBoard::isValidPos(const std::pair<int, int> pos) const {
        return (pos.first >= 0 && pos.second >= 0 && pos.first < size && pos.second < size);
    }

    /**
     * @brief Bounds-checked cell read.
     */
    Piece HexBoard::getPieceAtPos(const std::pair<int, int> pos) const {
        if (!isValidPos(pos)) throw std::invalid_argument("Invalid Pos");
        return cells[static_cast<std::size_t>(pos.first * size + pos.second)];
    }

    /**
     * @brief Collects the coordinates of every cell holding the given piece.
     */
    std::vector<std::pair<int, int>> HexBoard::getPosByPiece(const Piece p) const {
        std::vector<std::pair<int, int>> res;
        res.reserve(static_cast<std::size_t>(countPiece(p)));

        for (int r = 0; r < size; ++r)
            for (int c = 0; c < size; ++c)
                if (cells[static_cast<std::size_t>(r * size + c)] == p) res.emplace_back(r, c);

        return res;
    }

    int HexBoard::countPiece(const Piece p) const {
        int n = 0;
        for (const Piece cell : cells) if (cell == p) ++n;
        return n;
    }

    /**
     * @brief Returns the in-bounds neighbours of a cell on the hexagonal grid.
     */
    std::vector<std::pair<int, int>> HexBoard::getAdjacentPos(const std::pair<int, int> pos) const {
        std::vector<std::pair<int, int>> adj;
        adj.reserve(6);

        for (int k = 0; k < 6; ++k) {
            if (std::pair<int, int> n = {pos.first + NEIGHBOUR_DR[k], pos.second + NEIGHBOUR_DC[k]};
                isValidPos(n)) {
                adj.push_back(n);
            }
        }
        return adj;
    }

    // --- Mutation ------------------------------------------------------------

    /**
     * @brief Places a stone and folds it into the connectivity structure.
     */
    void HexBoard::addPiece(const Piece p, const std::pair<int, int> pos) {
        if (!isValidPos(pos)) throw std::invalid_argument("Invalid addPiece");

        const int idx = pos.first * size + pos.second;
        if (cells[static_cast<std::size_t>(idx)] != Piece::EMPTY)
            throw std::invalid_argument("Invalid addPiece");

        cells[static_cast<std::size_t>(idx)] = p;
        connect(idx, p);
    }

    void HexBoard::blockCell(const std::pair<int, int> pos) {
        if (!isValidPos(pos)) throw std::invalid_argument("Invalid blockCell");

        const int idx = pos.first * size + pos.second;
        if (cells[static_cast<std::size_t>(idx)] != Piece::EMPTY)
            throw std::invalid_argument("Invalid blockCell");

        // No connect() call: a walled cell joins neither edges nor neighbours,
        // which is precisely what makes it a hole in the graph rather than a
        // stone owned by someone.
        cells[static_cast<std::size_t>(idx)] = Piece::BLOCKED;
    }

    /**
     * @brief Clears a cell and returns the stone it held, EMPTY if none.
     */
    Piece HexBoard::deletePiece(const std::pair<int, int> pos) {
        if (!isValidPos(pos)) return Piece::EMPTY;

        const std::size_t idx = static_cast<std::size_t>(pos.first * size + pos.second);
        const Piece p = cells[idx];
        if (p == Piece::EMPTY) return Piece::EMPTY;

        cells[idx] = Piece::EMPTY;
        rebuildConnectivity();   // a union cannot be undone, only rebuilt
        return p;
    }

    /**
     * @brief Applies the pie rule: recolour the stone and mirror it across the
     * main diagonal.
     */
    void HexBoard::swapPiece(const std::pair<int, int> pos) {
        if (!isValidPos(pos)) throw std::invalid_argument("Invalid Swap Pos");

        const std::size_t from = static_cast<std::size_t>(pos.first * size + pos.second);
        const Piece p = cells[from];
        if (p == Piece::EMPTY) throw std::invalid_argument("Cannot swap an empty position");

        // Transposed position, opposite colour.
        const std::size_t to = static_cast<std::size_t>(pos.second * size + pos.first);
        const Piece newColor = (p == Piece::RED_DISC) ? Piece::BLUE_DISC : Piece::RED_DISC;

        // On the diagonal from == to, so the swap degenerates to a recolour.
        cells[from] = Piece::EMPTY;
        cells[to] = newColor;

        rebuildConnectivity();
    }

    /**
     * @brief Compares the two edge roots of the player: equal means connected.
     */
    bool HexBoard::checkWin(const Player player) const {
        if (size == 0) return false;

        return player == Player::RED
            ? findSet(redTop()) == findSet(redBottom())
            : findSet(blueLeft()) == findSet(blueRight());
    }
}
