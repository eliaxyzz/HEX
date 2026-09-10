/**
 * @file move.h
 * @brief Core value types describing board contents, players and moves.
 */

#ifndef MOVE_H
#define MOVE_H

#include <cstdint>
#include <utility>

namespace hex {

    /**
     * @brief Cell content, doubling as player colour.
     */
    enum class Piece : std::uint8_t {
        RED_DISC,   ///< Red stone (player 1).
        BLUE_DISC,  ///< Blue stone (player 2).
        EMPTY,      ///< Playable empty cell.

        /**
         * @brief Walled cell: the Arcade mode black hole.
         *
         * Modelled as a cell state rather than a third pseudo-player because
         * legality checks (Situation::isValid, HexBoard::addPiece) and move
         * generation (forEachEmpty) already gate on EMPTY. Any non-EMPTY state is
         * therefore excluded from both paths without a single extra branch. A
         * pseudo-player would instead have to be filtered out by hand in every
         * colour-aware routine: connectivity, win detection, counters, MCTS
         * rollouts.
         *
         * @note Deliberately last in the enum: the preceding values are used in
         * comparisons and lookup tables and must not shift.
         */
        BLOCKED
    };

    /**
     * @brief Player identity, used both as turn marker and as colour.
     */
    enum class Player {
        RED,   ///< Player 1: connects the top edge to the bottom one.
        BLUE   ///< Player 2: connects the left edge to the right one.
    };

    /**
     * @brief Maps a player to its stone.
     */
    constexpr Piece pieceOf(const Player p) {
        return p == Player::RED ? Piece::RED_DISC : Piece::BLUE_DISC;
    }

    /**
     * @brief Maps a player to its opponent.
     */
    constexpr Player opponent(const Player p) {
        return p == Player::RED ? Player::BLUE : Player::RED;
    }

    /**
     * @brief Physical effect of a move on the board data.
     */
    enum class ActionKind {
        ADD,      ///< Place a stone on an empty cell.
        SWAP,     ///< Recolour an existing stone, used by the pie rule.
        NOTHING   ///< No board mutation: default construction or resignation.
    };

    /**
     * @brief Move category according to the game rules.
     */
    enum class MoveKind {
        ADD,    ///< Standard stone placement.
        PIE,    ///< Pie rule swap, legal on the second turn only.
        RESIGN  ///< Player concedes the match.
    };

    /**
     * @brief Low-level data required to mutate the board.
     */
    struct Action {
        ActionKind kind;              ///< Operation to perform.
        Piece piece;                  ///< Stone involved.
        std::pair<int, int> position; ///< Target (row, column).

        /** @brief Builds a no-op action. */
        Action() : kind(ActionKind::NOTHING), piece(Piece::EMPTY), position({-1, -1}) {}

        /** @brief Builds a fully specified action. */
        Action(ActionKind k, Piece p, const std::pair<int, int> &pos) : kind(k), piece(p), position(pos) {}

        /** @brief Member-wise equality. */
        bool operator==(const Action& other) const {
            return kind == other.kind && piece == other.piece && position == other.position;
        }
    };

    /**
     * @brief Complete move, exchanged between players and the game engine.
     */
    struct Move {
        MoveKind kind;  ///< Rule applied by this move.
        Action action;  ///< Board mutation it entails.

        /**
         * @brief Builds a resignation.
         * @note RESIGN is the safe default: a player returning a default
         * constructed Move forfeits instead of triggering undefined behaviour.
         */
        Move() : kind(MoveKind::RESIGN), action() {}

        /**
         * @brief Builds a move from its rule category and board action.
         * @param k Move category.
         * @param a Associated board action.
         */
        Move(MoveKind k, Action a) : kind(k), action(std::move(a)) {}

        /** @brief Member-wise equality. */
        bool operator==(const Move& other) const {
            return kind == other.kind && action == other.action;
        }
    };
}
#endif //MOVE_H