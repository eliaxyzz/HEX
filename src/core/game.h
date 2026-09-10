/**
 * @file game.h
 * @brief Situation: immutable game state and rule enforcement.
 */

#ifndef GAME_H
#define GAME_H

#include "core/board.h"
#include "core/move.h"
#include <optional>
#include <vector>

namespace hex {

    /**
     * @brief Overall outcome of a match.
     */
    enum class GameStatus {
        IN_PROGRESS, ///< Match still running.
        RED_WON,     ///< Red has won.
        BLUE_WON     ///< Blue has won.
    };

    /**
     * @brief Reason a match ended.
     * @note Presentation concern: the UI must tell a win by connection apart from
     * one by resignation or timeout.
     */
    enum class EndReason {
        NONE,        ///< Match still running.
        CONNECTION,  ///< Winner linked its two opposite edges.
        RESIGN,      ///< Opponent conceded.
        TIMEOUT,     ///< Opponent exceeded its time budget.
        ILLEGAL_MOVE ///< Opponent proposed an illegal move.
    };

    /** @brief Maps a winner to the matching terminal status. */
    constexpr GameStatus statusFor(const Player winner) {
        return winner == Player::RED ? GameStatus::RED_WON : GameStatus::BLUE_WON;
    }

    /** @brief Extracts the winner from a status, nullopt while in progress. */
    constexpr std::optional<Player> winnerOf(const GameStatus status) {
        switch (status) {
            case GameStatus::RED_WON:  return Player::RED;
            case GameStatus::BLUE_WON: return Player::BLUE;
            default:                   return std::nullopt;
        }
    }

    /**
     * @brief Outcome of a completed match: who won and why.
     */
    struct GameResult {
        Player winner;
        EndReason reason;
    };

    /**
     * @brief Immutable snapshot of a game: board, side to move and rule flags.
     *
     * Transitions produce new instances through next(), which keeps every state
     * reachable by the search shareable and free of aliasing hazards.
     */
    class Situation {
    private:
        /** @brief Board state at this point in the game. */
        HexBoard board;

        /**
         * @brief Side to move.
         * @note Advances on every move including the winning one, so it reads BLUE
         * after Red connects. Only meaningful while !isOver().
         */
        Player to_move{Player::RED};

        /** @brief Match status: in progress, won by Red, or won by Blue. */
        GameStatus status{GameStatus::IN_PROGRESS};

        /** @brief Termination reason, NONE while the match is running. */
        EndReason end_reason{EndReason::NONE};

        /** @brief Whether the pie rule is playable on this turn. */
        bool pie_rule{};

        /** @brief Internal constructor for the states produced by next(). */
        Situation(HexBoard b, Player toMove, GameStatus st, EndReason reason);

    public:
        /**
         * @brief Builds a running state, deriving pie rule availability from the
         * board contents.
         * @param b Board layout.
         * @param toMove Side to move.
         */
        Situation(const HexBoard& b, Player toMove);

        /** @brief Builds an empty state. */
        Situation() = default;

        /**
         * @brief Returns the side to move.
         * @note Only meaningful while !isOver(); on a finished game it names whoever
         * would have moved next.
         */
        [[nodiscard]] Player toMove() const { return to_move; }

        /** @brief Returns the match status. */
        [[nodiscard]] GameStatus getStatus() const { return status; }

        /** @brief Returns the termination reason, NONE while running. */
        [[nodiscard]] EndReason getEndReason() const { return end_reason; }

        /** @brief Tests whether the match has ended. */
        [[nodiscard]] bool isOver() const { return status != GameStatus::IN_PROGRESS; }

        /** @brief Returns the winner once the match has ended. */
        [[nodiscard]] std::optional<Player> winner() const { return winnerOf(status); }

        /**
         * @brief Returns a copy of the underlying board.
         */
        [[nodiscard]] HexBoard getBoard() const { return board; }

        /**
         * @brief Returns the board as a stone matrix, for rendering.
         */
        [[nodiscard]] std::vector<std::vector<Piece>> getBoardView() const { return board.getBoardView(); }

        /**
         * @brief Validates a move against the current state.
         *
         * Rejects moves on a finished game, moves of the wrong colour, and
         * placements on a cell that is not empty. A pie move is accepted only while
         * the rule is active and targets the single red stone.
         *
         * @param m Move to validate.
         * @return true if the move is legal.
         */
        [[nodiscard]] bool isValid(Move m) const;

        /**
         * @brief Enumerates every legal move from this state.
         * @return Placements on the empty cells, plus the pie swap when available.
         */
        [[nodiscard]] std::vector<Move> validMoves() const;

        /**
         * @brief Applies a move and returns the resulting state.
         *
         * Leaves this instance untouched; turn advance and win detection are handled
         * as part of the transition.
         *
         * @param m Move to apply.
         * @return State reached after the move.
         * @throw std::invalid_argument If the move is illegal.
         */
        [[nodiscard]] Situation next(Move m) const;

        /**
         * @brief Builds the terminal state produced by an event outside the rules,
         * such as a timeout awarded by the referee.
         * @param winner Player awarded the win.
         * @param reason Termination reason.
         */
        [[nodiscard]] Situation endedBy(Player winner, EndReason reason) const;

        /**
         * @brief Tests whether the pie rule can be invoked on this turn, i.e. on
         * Blue's first move.
         */
        [[nodiscard]] bool isPieRuleActive() const { return pie_rule; }
    };
}

#endif //GAME_H