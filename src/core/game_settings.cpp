/**
 * @file game_settings.cpp
 * @brief Game settings implementation: derivations from the menu choices.
 */

#include "core/game_settings.h"

namespace hexapp {

    hex::MCTSConfig configFor(const Difficulty d) {
        // The two lower levels stay on a single tree. Root parallelisation
        // multiplies playouts at equal time, so enabling it everywhere would make
        // Easy as strong as Hard used to be: the difficulty scale is a game design
        // decision, not a function of the host's core count. Hard takes whatever
        // the machine offers, which is what the label promises.
        switch (d) {
            case Difficulty::EASY:
                return {.time_limit_ms = 100.0,  .save_bridges = false, .threads = 1};
            case Difficulty::MEDIUM:
                return {.time_limit_ms = 500.0,  .save_bridges = false, .threads = 1};
            case Difficulty::HARD:
                return {.time_limit_ms = 2000.0, .save_bridges = true,  .threads = 0};
        }
        return {};
    }

    hexui::StringKey labelOf(const GameMode m) {
        return m == GameMode::HUMAN_VS_AI ? hexui::StringKey::MENU_MODE_HUMAN_COMPUTER
                                          : hexui::StringKey::MENU_MODE_HUMAN_HUMAN;
    }

    hexui::StringKey labelOf(const Difficulty d) {
        switch (d) {
            case Difficulty::EASY:   return hexui::StringKey::MENU_DIFFICULTY_EASY;
            case Difficulty::MEDIUM: return hexui::StringKey::MENU_DIFFICULTY_MEDIUM;
            case Difficulty::HARD:   return hexui::StringKey::MENU_DIFFICULTY_HARD;
        }
        return hexui::StringKey::MENU_DIFFICULTY_MEDIUM;
    }

    GameSettings rematchOf(const GameSettings& s) {
        GameSettings next = s;

        if (s.mode == GameMode::HUMAN_VS_HUMAN) {
            // The first name is Red by convention, so swapping names swaps colours.
            next.player_name = s.opponent_name;
            next.opponent_name = s.player_name;
            return next;
        }

        next.human_colour = hex::opponent(s.human_colour);
        return next;
    }

    hexplay::GameClock clockFor(const GameSettings& s) {
        // Arcade does not shorten the match budget, it changes its unit: ten
        // seconds per move, reloaded on every turn.
        if (s.arcade)
            return hexplay::GameClock(hexplay::ARCADE_TURN_SECONDS,
                                      hexplay::ClockMode::PER_TURN);

        return hexplay::GameClock(s.seconds_per_player, hexplay::ClockMode::TOTAL);
    }

    bool isHumanColour(const GameSettings& s, const hex::Player p) {
        // Both colours are human in a local match; only the selected one otherwise.
        return s.mode == GameMode::HUMAN_VS_HUMAN || p == s.human_colour;
    }

    namespace {
        /** @brief Name of the human who configured the match. */
        std::string humanName(const GameSettings& s, const hexui::LocalizationManager& loc) {
            return s.player_name.empty() ? loc.text(hexui::StringKey::NAME_HUMAN) : s.player_name;
        }

        /** @brief Name of the opponent, human or engine. */
        std::string opponentName(const GameSettings& s, const hexui::LocalizationManager& loc) {
            if (!s.opponent_name.empty()) return s.opponent_name;
            if (s.mode == GameMode::HUMAN_VS_HUMAN)
                return loc.text(hexui::StringKey::NAME_HUMAN_2);

            return loc.text(hexui::StringKey::NAME_COMPUTER) + " "
                 + loc.text(labelOf(s.difficulty));
        }
    }

    std::string nameOfColour(const GameSettings& s, const hex::Player p,
                             const hexui::LocalizationManager& loc) {
        // Between two humans the first name is Red by convention: whoever set the
        // match up also moves first.
        if (s.mode == GameMode::HUMAN_VS_HUMAN) {
            return p == hex::Player::RED ? humanName(s, loc) : opponentName(s, loc);
        }

        // Against the engine the name follows the chosen colour, not the turn
        // order, so a human playing Blue keeps their own name.
        return p == s.human_colour ? humanName(s, loc) : opponentName(s, loc);
    }

    std::string firstPlayerName(const GameSettings& s, const hexui::LocalizationManager& loc) {
        return nameOfColour(s, hex::Player::RED, loc);
    }

    std::string secondPlayerName(const GameSettings& s, const hexui::LocalizationManager& loc) {
        return nameOfColour(s, hex::Player::BLUE, loc);
    }
}
