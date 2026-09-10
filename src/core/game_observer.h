/**
 * @file game_observer.h
 * @brief Observer interface for match events.
 */

#ifndef GAME_OBSERVER_H
#define GAME_OBSERVER_H

#include "core/game.h"
#include "core/move.h"

#include <optional>
#include <string>

namespace hex {

    /**
     * @brief Listener for the events emitted by GameController.
     *
     * Every hook has an empty default, so an implementation only overrides the
     * events it cares about.
     */
    class GameObserver {
    public:
        virtual ~GameObserver() = default;

        /**
         * @brief Signals that the match is about to start.
         * @param initial Initial state: empty board, Red to move.
         */
        virtual void onGameStart(const Situation& initial) { (void)initial; }

        /**
         * @brief Signals that a move is about to be requested from a player.
         * @param current State before the move.
         * @param mover Side to move.
         * @param playerName Display name of the side to move.
         */
        virtual void onTurnStart(const Situation& current, Player mover, const std::string& playerName) {
            (void)current; (void)mover; (void)playerName;
        }

        /**
         * @brief Signals that a legal move has been applied.
         * @param before State before the move.
         * @param m Move applied.
         * @param after Resulting state.
         */
        virtual void onMove(const Situation& before, const Move& m, const Situation& after) {
            (void)before; (void)m; (void)after;
        }

        /**
         * @brief Signals an illegal move, which forfeits the match.
         * @param current State in which the move was proposed.
         * @param mover Offending player.
         * @param m Rejected move.
         * @param playerName Display name of the offending player.
         */
        virtual void onInvalidMove(const Situation& current, Player mover, const Move& m,
                                   const std::string& playerName) {
            (void)current; (void)mover; (void)m; (void)playerName;
        }

        /**
         * @brief Signals a timeout, which forfeits the match.
         * @param current State in which the time ran out.
         * @param mover Player that timed out.
         * @param playerName Display name of that player.
         */
        virtual void onTimeout(const Situation& current, Player mover, const std::string& playerName) {
            (void)current; (void)mover; (void)playerName;
        }

        /**
         * @brief Signals that move computation threw, which forfeits the match.
         * @param current State in which the failure occurred.
         * @param mover Player whose computation threw.
         * @param playerName Display name of that player.
         * @param what Exception message.
         */
        virtual void onPlayerError(const Situation& current, Player mover,
                                   const std::string& playerName, const std::string& what) {
            (void)current; (void)mover; (void)playerName; (void)what;
        }

        /**
         * @brief Signals that the last move, or a forfeit, has been undone.
         * @param restored Restored state: board, side to move and match status are
         * all consistent again with that point in the game.
         * @param undone Move that was undone, or nullopt when the undone event was a
         * referee decision such as a timeout, an illegal move or an error.
         */
        virtual void onUndo(const Situation& restored, const std::optional<Move>& undone) {
            (void)restored; (void)undone;
        }

        /**
         * @brief Signals that the match has ended.
         * @param final Final state.
         * @param result Winner and termination reason.
         */
        virtual void onGameEnd(const Situation& final, const GameResult& result) {
            (void)final; (void)result;
        }
    };
}

#endif //GAME_OBSERVER_H
