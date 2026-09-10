/**
 * @file deferred_player.h
 * @brief Player whose move is supplied from outside, within the turn window.
 *
 * Shared behaviour of the human at the window and the remote player at the far
 * end of a socket: neither can produce a move on demand, both deliver one when it
 * arrives, and in both cases it must be accepted only at the right moment.
 *
 * A move is accepted only between startMove() and tryTakeMove(). Outside that
 * window it is dropped rather than queued: a move buffered while the opponent is
 * thinking would go stale and later be played into a position where it may be
 * illegal.
 *
 * Every move goes through Situation::isValid() before being queued, so the engine
 * never sees an illegal move and cannot end the match with ILLEGAL_MOVE because of
 * a stray click or a malicious packet. Keeping that in one class is what stops
 * input and network paths from enforcing two different rules.
 */

#ifndef DEFERRED_PLAYER_H
#define DEFERRED_PLAYER_H

#include <optional>
#include <string>

#include "core/game.h"
#include "core/player.h"

namespace hexplay {

    /** @brief Base class for players fed from outside the engine. */
    class DeferredPlayer : public hex::AbstractPlayer {
    public:
        /**
         * @brief Builds the player.
         * @param name Name shown in the interface and in logs.
         */
        explicit DeferredPlayer(std::string name);

        // --- AbstractPlayer: identity ---

        std::string getName() override { return name; }

        /**
         * @brief Unsupported here: the synchronous contract cannot be honoured.
         * @throw std::logic_error Always. A player fed from outside cannot produce a
         * move on demand, which is precisely why the asynchronous contract exists.
         */
        hex::Move getMoveFromSit(hex::Situation situation) override;

        // --- AbstractPlayer: asynchronous contract ---

        /** @brief Opens the turn window: the engine is now asking for a move. */
        void startMove(const hex::Situation& situation) override;

        /** @brief Hands over the move once it has arrived, nullopt otherwise. */
        std::optional<hex::Move> tryTakeMove() override;

        /** @brief Cancels the turn: closes the window and drops the queued move. */
        void abortMove() override;

        // --- Caller-facing API ---

        /** @brief Tests whether the turn window is open. */
        [[nodiscard]] bool isArmed() const { return armed; }

        /**
         * @brief Returns the position the move was requested for.
         * @note Only meaningful while isArmed().
         */
        [[nodiscard]] const hex::Situation& askedSituation() const { return current; }

        /** @brief Tests whether a move is already queued for collection. */
        [[nodiscard]] bool hasQueuedMove() const { return queued.has_value(); }

        /**
         * @brief Tests whether the given move would be accepted right now.
         * @note Exactly the condition submit() applies, without the side effect, so
         * a caller can preview only where a move would really land instead of
         * reimplementing the rule.
         */
        [[nodiscard]] bool wouldAccept(const hex::Move& move) const;

        /**
         * @brief Queues the move if it is acceptable.
         * @return true when accepted; false if the turn window is closed, a move is
         * already queued, or the move is illegal in the current position.
         */
        bool submit(const hex::Move& move);

        /**
         * @brief Returns the pie swap move, if legal on this turn.
         * @note Extracted once from Situation::validMoves() in startMove(), keeping
         * the rule owned by the engine rather than duplicated by the UI.
         */
        [[nodiscard]] const std::optional<hex::Move>& swapMove() const { return swap_move; }

    private:
        /** @brief Display name. */
        std::string name;

        /** @brief True between startMove() and tryTakeMove(): the turn window. */
        bool armed = false;

        /** @brief Position the move was requested for. */
        hex::Situation current;

        /** @brief Received move, waiting for the engine to collect it. */
        std::optional<hex::Move> queued;

        /** @brief Pie swap move, when legal in the current position. */
        std::optional<hex::Move> swap_move;
    };
}

#endif //DEFERRED_PLAYER_H
