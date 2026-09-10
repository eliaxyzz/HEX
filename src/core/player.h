/**
 * @file player.h
 * @brief Player interface and the MCTS-backed concrete implementation.
 */

#ifndef PLAYER_H
#define PLAYER_H

#include <future>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include "core/game.h"
#include "core/mcts.h"

namespace hex {

    /**
     * @brief Abstract Hex player exposing two overlapping contracts.
     *
     * - **Synchronous** (getMoveFromSit): compute a move now. Implemented by
     *   engines, may block for as long as it needs.
     * - **Asynchronous** (startMove / tryTakeMove / abortMove): start thinking and
     *   poll for the result later. Used by GameController and never blocking.
     *
     * The default asynchronous implementation adapts the synchronous one: startMove
     * dispatches the computation to a worker thread and tryTakeMove returns nullopt
     * while it is still running. An engine therefore needs no changes to run under
     * a GUI. A natively asynchronous player (a human at a window, a remote engine)
     * overrides the three asynchronous methods and leaves the worker unused.
     *
     * @note The worker is never detached: abortMove() requests cancellation and
     * joins, and so does the destructor. Keeping that join short requires the
     * computation to be cooperative, i.e. to poll currentStopToken() periodically,
     * as the MCTS engine does. Ignoring the token stays correct but makes
     * abortMove() as slow as a full search.
     */
    class AbstractPlayer {
    public:
        /**
         * @brief Returns the display name used in logs and on screen.
         */
        virtual std::string getName() = 0;

        /**
         * @brief Returns the author identifiers, empty when unspecified.
         */
        virtual std::string getIDs() { return ""; }

        /**
         * @brief Synchronous contract: computes a move for the given position.
         * @param situation Current game state.
         * @return Chosen move.
         * @note May block indefinitely; invoked on a worker thread.
         */
        virtual Move getMoveFromSit(Situation situation) = 0;

        /**
         * @brief Starts the search for the given position and returns immediately.
         *
         * The default implementation spawns a worker running getMoveFromSit. Any
         * request still in flight is cancelled and joined first, so a player owns
         * at most one worker.
         *
         * @param situation Current game state.
         */
        virtual void startMove(const Situation& situation);

        /**
         * @brief Polls for the move started by startMove without blocking.
         * @return The move once ready, nullopt while the player is still deciding.
         * @throw std::exception Rethrows whatever the player threw, exactly once.
         */
        virtual std::optional<Move> tryTakeMove();

        /**
         * @brief Cancels the pending request and joins its worker.
         *
         * Guarantees that no thread touches this object once it returns. The wait is
         * negligible for a cooperative search such as the MCTS engine.
         */
        virtual void abortMove();

        /**
         * @brief Shuts down any request still in flight.
         * @warning The shutdown runs in the base destructor, hence after the derived
         * one. Never destroy a player while a request is pending: call abortMove()
         * first, as GameController does on every path including timeout.
         */
        virtual ~AbstractPlayer();

    protected:
        /**
         * @brief Cancellation token of the pending request.
         * @note A long search should poll it and return the best result found so far.
         */
        [[nodiscard]] std::stop_token currentStopToken() const { return stop.get_token(); }

    private:
        /** @brief Cancellation source of the current request. */
        std::stop_source stop;

        /** @brief Worker thread running the synchronous contract. */
        std::thread worker;

        /** @brief Channel through which the worker delivers the move. */
        std::future<Move> pending;
    };

    /**
     * @brief Computer player backed by the Monte Carlo tree search engine.
     *
     * Thin wrapper around MCTSPlayer: receives the position from GameController and
     * delegates the decision to the search.
     *
     * @note Implements the synchronous contract only, inheriting the asynchronous
     * adaptation from AbstractPlayer. The stop token is forwarded to the engine so
     * cancellation is honoured promptly.
     */
    class HexPlayer : public AbstractPlayer {
    private:
        /** @brief Owned search engine instance. */
        MCTSPlayer mcts;

    public:
        /**
         * @brief Builds the player with the given search configuration.
         * @param cfg Search budget and exploration constant.
         */
        explicit HexPlayer(const MCTSConfig& cfg = {}) : mcts(cfg) {}

        /**
         * @brief Returns the display name.
         * @note "Computer" rather than the algorithm name: this string reaches the
         * turn announcement and the console status bar, where the search family is
         * meaningless to the player.
         */
        std::string getName() override { return "Computer"; }

        /**
         * @brief Returns the author identifiers.
         */
        std::string getIDs() override { return "123456"; }

        /**
         * @brief Computes the best move through the MCTS engine.
         * @param situation Current game state.
         * @return Most robust move found by the search.
         */
        Move getMoveFromSit(Situation situation) override {
            return mcts.getMove(situation, currentStopToken());
        }
    };
}

#endif //PLAYER_H
