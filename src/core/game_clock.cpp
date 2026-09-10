/**
 * @file game_clock.cpp
 * @brief Game clock implementation.
 */

#include "core/game_clock.h"

#include <algorithm>
#include <cmath>

namespace hexplay {

    GameClock::GameClock(const double seconds_per_player, const ClockMode mode)
        : budget(seconds_per_player),
          clock_mode(mode),
          red_left(std::max(0.0, seconds_per_player)),
          blue_left(std::max(0.0, seconds_per_player)) {}

    void GameClock::setRunning(const std::optional<hex::Player> p) {
        // Reload on turn change only: setRunning is called every frame with the
        // same player, and reloading each time would pin the count at its maximum.
        const bool changed = p.has_value() && p != running;

        running = p;

        if (clock_mode == ClockMode::PER_TURN && changed) {
            double& left = (*p == hex::Player::RED) ? red_left : blue_left;
            left = std::max(0.0, budget);
        }
    }

    void GameClock::tick(const double dt) {
        if (!enabled() || !running || dt <= 0.0) return;

        double& left = (*running == hex::Player::RED) ? red_left : blue_left;
        left = std::max(0.0, left - dt);
    }

    double GameClock::remaining(const hex::Player p) const {
        return p == hex::Player::RED ? red_left : blue_left;
    }

    bool GameClock::expired(const hex::Player p) const {
        return enabled() && remaining(p) <= 0.0;
    }

    std::optional<hex::Player> GameClock::expiredPlayer() const {
        if (expired(hex::Player::RED)) return hex::Player::RED;
        if (expired(hex::Player::BLUE)) return hex::Player::BLUE;
        return std::nullopt;
    }

    std::string GameClock::format(const double seconds) {
        // Round up: 0.2 seconds left is still a second to play, and displaying
        // 00:00 before expiry would misreport the state.
        const double clamped = std::clamp(seconds, 0.0, 99.0 * 60.0 + 59.0);
        const auto total = static_cast<long long>(std::ceil(clamped));

        const long long minutes = total / 60;
        const long long secs = total % 60;

        std::string out;
        out.reserve(5);
        if (minutes < 10) out += '0';
        out += std::to_string(minutes);
        out += ':';
        if (secs < 10) out += '0';
        out += std::to_string(secs);
        return out;
    }

    std::string GameClock::formatTenths(const double seconds) {
        const double clamped = std::clamp(seconds, 0.0, 99.9);

        // Truncate to a tenth so the display is never more generous than the
        // time actually left.
        const auto tenths = static_cast<long long>(std::floor(clamped * 10.0));

        std::string out;
        out.reserve(4);
        out += std::to_string(tenths / 10);
        out += '.';
        out += std::to_string(tenths % 10);
        return out;
    }
}
