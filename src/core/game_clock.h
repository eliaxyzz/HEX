/**
 * @file game_clock.h
 * @brief Chess-style game clock holding one budget per player.
 *
 * Only the side to move burns time, and the count stops when the turn passes.
 * Engine thinking time is charged to the engine exactly as a human's deliberation
 * is charged to the human.
 *
 * @note Deliberately distinct from the per-move deadline enforced by
 * GameController: that one exists to referee an unresponsive engine, this one is
 * an overall budget the player spends as it sees fit. Two different rules, two
 * different objects; expiry is reported to the controller by whoever polls the
 * clock.
 * @note SFML-free and wall-clock free: time enters through tick(dt), so a test
 * can advance ten minutes in a microsecond.
 */

#ifndef GAME_CLOCK_H
#define GAME_CLOCK_H

#include <optional>
#include <string>

#include "core/move.h"

namespace hexplay {

    /** @brief Default per-player budget, in seconds. */
    inline constexpr double DEFAULT_CLOCK_SECONDS = 600.0;

    /**
     * @brief How the budget is spent.
     *
     * A clock parameter rather than a second clock class: the state, the consumer
     * (tick) and the expiry check (expiredPlayer) are identical in both modes, and
     * only the reset point differs.
     */
    enum class ClockMode {
        /** @brief One budget for the whole match, like a chess clock. */
        TOTAL,

        /**
         * @brief One budget per move, reset on every turn change.
         * Sudden death, as used by Arcade mode.
         */
        PER_TURN
    };

    /** @brief Two countdowns, at most one of which is running. */
    class GameClock {
    public:
        /**
         * @brief Builds the clock.
         * @param seconds_per_player Budget granted to each player. A value <= 0
         * disables the clock entirely: time never runs and never expires, which is
         * the correct behaviour for an untimed match.
         * @param mode Whether the budget covers the match or a single move.
         */
        explicit GameClock(double seconds_per_player = DEFAULT_CLOCK_SECONDS,
                           ClockMode mode = ClockMode::TOTAL);

        /** @brief Returns how the budget is spent. */
        [[nodiscard]] ClockMode mode() const { return clock_mode; }

        /** @brief Tests whether the clock is enabled. */
        [[nodiscard]] bool enabled() const { return budget > 0.0; }

        /** @brief Returns the initial per-player budget. */
        [[nodiscard]] double budgetSeconds() const { return budget; }

        /**
         * @brief Selects whose time is running; nullopt pauses both.
         *
         * In PER_TURN mode, switching to a different player reloads that player's
         * budget. The reset lives here because this is the only point at which the
         * clock learns the turn changed: callers keep reporting the side to move and
         * never have to remember to reload. Repeated calls naming the same player do
         * not reload, otherwise the count would never drop.
         *
         * @param p Side to move, or nullopt to pause, as needed once the match is
         * over or while a menu is up.
         */
        void setRunning(std::optional<hex::Player> p);

        /** @brief Returns the player whose time is running, if any. */
        [[nodiscard]] const std::optional<hex::Player>& runningPlayer() const { return running; }

        /**
         * @brief Charges `dt` seconds to the running player.
         * @param dt Elapsed seconds; non-positive values are ignored so the clock
         * can never hand time back. The remainder is clamped at zero.
         */
        void tick(double dt);

        /** @brief Returns the seconds left to the given player. */
        [[nodiscard]] double remaining(hex::Player p) const;

        /** @brief Tests whether the given player has run out of time. */
        [[nodiscard]] bool expired(hex::Player p) const;

        /**
         * @brief Returns the player who ran out of time, if any.
         * @note A disabled clock never expires.
         */
        [[nodiscard]] std::optional<hex::Player> expiredPlayer() const;

        /**
         * @brief Formats a remainder as MM:SS.
         * @note Rounds up, so a fraction of a second still displays as 1: showing
         * zero on a clock that has not expired would misreport the state. Saturates
         * past 99 minutes and treats negative values as zero.
         */
        [[nodiscard]] static std::string format(double seconds);

        /**
         * @brief Formats a short remainder as S.d, for example 9.4.
         *
         * A ten second Blitz budget reads as a frozen display in MM:SS, since it
         * sits on 00:10 for a full second. Tenths always move and convey the
         * urgency.
         *
         * @note Truncates rather than rounding up, the opposite of format(): here
         * 0.0 appears only once the time is really gone, and showing 0.1 on a lost
         * game would be the mirror-image lie.
         */
        [[nodiscard]] static std::string formatTenths(double seconds);

    private:
        /** @brief Seconds granted to each player; <= 0 disables the clock. */
        double budget;

        /** @brief Whether the budget covers the match or a single move. */
        ClockMode clock_mode;

        /** @brief Remaining seconds for Red and for Blue. */
        double red_left;
        double blue_left;

        /** @brief Player currently burning time. */
        std::optional<hex::Player> running;
    };
}

#endif //GAME_CLOCK_H
