/**
 * @file game_controller.h
 * @brief Game engine driving a match one turn at a time.
 */

#ifndef GAME_CONTROLLER_H
#define GAME_CONTROLLER_H

#include "core/game.h"
#include "core/game_observer.h"
#include "core/player.h"

#include <chrono>
#include <optional>
#include <vector>

namespace hex {

    /**
     * @brief Outcome of a single GameController::step() call.
     */
    enum class StepResult {
        PLAYED,    ///< A turn was played, possibly ending the match.
        WAITING,   ///< The side to move is still deciding; nothing changed.
        GAME_OVER  ///< The match had already ended; nothing left to do.
    };

    /**
     * @brief Drives a match between two players, one turn at a time.
     *
     * step() advances at most one turn and hands control straight back. A console
     * program can call run(), which loops for convenience; a GUI calls step() from
     * its own event loop and never yields its thread to a loop it does not own.
     *
     * The controller produces no output: everything that happens is reported to the
     * registered observers.
     *
     * @see GameObserver
     */
    class GameController {
    public:
        /**
         * @brief Builds a match on an empty n x n board.
         * @param size Board side length.
         * @param red Player moving first.
         * @param blue Second player.
         * @param seconds_per_move Per-move deadline; <= 0 disables it and runs the
         * computation on the calling thread, with no worker and no detach.
         */
        GameController(int size, AbstractPlayer& red, AbstractPlayer& blue, int seconds_per_move = 0);

        /**
         * @brief Builds a match on a pre-arranged board.
         *
         * Required by variants that do not start from an empty board: in Arcade the
         * black holes are already walled off. Giving the controller an "Arcade mode"
         * flag instead would teach it a rule that is not its own; it referees turns
         * and does not decide how the table is set up.
         *
         * @param board Initial board, already in the intended state.
         * @param red Player moving first.
         * @param blue Second player.
         * @param seconds_per_move Per-move deadline; <= 0 disables it.
         */
        GameController(HexBoard board, AbstractPlayer& red, AbstractPlayer& blue,
                       int seconds_per_move = 0);

        /**
         * @brief Registers an observer.
         * @warning Non-owning: every observer must outlive the controller.
         */
        void addObserver(GameObserver& observer);

        /** @brief Returns the current game state. */
        [[nodiscard]] const Situation& situation() const { return sit; }

        /** @brief Tests whether the match has ended. */
        [[nodiscard]] bool isOver() const { return sit.isOver(); }

        /** @brief Returns the outcome, available only once the match has ended. */
        [[nodiscard]] std::optional<GameResult> result() const;

        /**
         * @brief Advances the match without ever blocking.
         *
         * On the first call of each turn it emits onTurnStart and asks the player to
         * start thinking. Every call then polls for the result and returns WAITING
         * immediately if it is not ready, leaving it to the caller, a GUI event loop
         * or run(), to call again later.
         *
         * Once the move arrives it is validated, applied and reported. An illegal
         * move, an expired deadline or an exception thrown by the player all end the
         * match in the opponent's favour.
         *
         * @return What happened during this call.
         */
        StepResult step();

        /**
         * @brief Ends the match on a timeout observed by an external referee.
         *
         * The controller polices the per-move deadline itself; a match clock
         * (hexplay::GameClock) is held by whoever drives the match and reported here
         * on expiry. Both paths converge on the same outcome: onTimeout, then a loss
         * with EndReason::TIMEOUT.
         *
         * Any move request in flight is cancelled, so no player is still thinking
         * once this returns.
         *
         * @param loser Player whose time ran out.
         * @return false if the match had already ended and there was nothing to close.
         */
        bool claimTimeout(Player loser);

        /**
         * @brief Tests whether there is anything to undo.
         * @return true once at least one move has been played, or when the match was
         * closed by the referee through a timeout, an illegal move or an error.
         */
        [[nodiscard]] bool canUndo() const;

        /**
         * @brief Undoes the last move, or the referee decision that ended the match.
         *
         * Restores the previous state in full: board, side to move, outcome and pie
         * rule all stay consistent because the exact Situation of that moment is
         * reinstated rather than reconstructed backwards. Undoing the winning move
         * puts the match back in progress.
         *
         * Any move request in flight is cancelled; the next step() asks the side to
         * move again.
         *
         * @return true if something was undone.
         */
        bool undo();

        /**
         * @brief Replays a move sequence from the initial position.
         *
         * Used when loading a saved game: the state is rebuilt by replaying the moves
         * instead of trusting a position stored in the file.
         *
         * Every move is checked with Situation::isValid. A save file is untrusted
         * input even when the game wrote it, since it may have been edited or
         * corrupted. If any move is illegal, or the sequence continues past the end
         * of the match, the whole replay is rejected and the controller is left
         * exactly as it was: nothing is applied halfway.
         *
         * @param sequence Moves in chronological order.
         * @return true if the entire sequence was applied.
         * @note Valid only on a controller still at the initial position and with no
         * move request in flight; otherwise it returns false without side effects.
         * Observers are not notified, since this runs before the match is observed.
         */
        bool replay(const std::vector<Move>& sequence);

        /** @brief Returns the number of moves played so far. */
        [[nodiscard]] std::size_t moveCount() const { return moves.size(); }

        /** @brief Returns the moves played, in chronological order. */
        [[nodiscard]] const std::vector<Move>& moveHistory() const { return moves; }

        /**
         * @brief Returns the positions visited, in chronological order.
         * @note positions()[0] is the initial position and positions()[i] the one
         * reached after moveHistory()[i - 1].
         */
        [[nodiscard]] const std::vector<Situation>& positions() const { return history; }

        /**
         * @brief Runs the match to completion by calling step() repeatedly.
         * @return Final outcome.
         * @note Console-oriented: it busy-waits on asynchronous players. A GUI must
         * call step() from its own event loop instead.
         */
        GameResult run();

    private:
        /** @brief Current game state. */
        Situation sit;

        /** @brief The two players, one per colour. Non-owning. */
        AbstractPlayer* red;
        AbstractPlayer* blue;

        /** @brief Per-move deadline in seconds; <= 0 means no limit. */
        int seconds_per_move;

        /** @brief Registered observers. Non-owning. */
        std::vector<GameObserver*> observers;

        /** @brief Whether onGameStart has already been emitted. */
        bool started = false;

        /** @brief Whether the side to move has already been asked for a move. */
        bool move_requested = false;

        /** @brief Instant the current turn's request was issued. */
        std::chrono::steady_clock::time_point turn_started_at{};

        /**
         * @brief Positions visited: history[0] is the initial one and every applied
         * move appends another. Referee decisions are not recorded here.
         */
        std::vector<Situation> history;

        /** @brief Moves played, parallel to history[1..]. */
        std::vector<Move> moves;

        /**
         * @brief True when the match was closed by the referee rather than by a move.
         * @note The current state is then absent from history, and undo() discards it
         * without consuming a move.
         */
        bool ended_by_event = false;

        /** @brief Returns the player holding the given colour. */
        [[nodiscard]] AbstractPlayer& playerFor(Player p) const;

        /**
         * @brief Ends the match in favour of the opponent of `loser` and emits
         * onGameEnd.
         */
        void finishWithLoss(Player loser, EndReason reason);

        /** @brief Forwards an event to every registered observer. */
        template <typename Fn>
        void notify(Fn&& fn) const {
            for (GameObserver* o : observers) fn(*o);
        }
    };
}

#endif //GAME_CONTROLLER_H
