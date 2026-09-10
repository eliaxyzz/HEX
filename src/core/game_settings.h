/**
 * @file game_settings.h
 * @brief Menu choices handed over to a match, and the derivations from them.
 *
 * @note SFML-free: plain data plus a handful of pure functions, so difficulty
 * mapping and default naming stay unit testable.
 */

#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H

#include <string>
#include <string_view>

#include "core/arcade.h"
#include "core/game_clock.h"
#include "core/localization.h"
#include "core/mcts.h"
#include "core/move.h"

namespace hexapp {

    /** @brief Who plays against whom. */
    enum class GameMode {
        HUMAN_VS_AI,    ///< One human against the engine.
        HUMAN_VS_HUMAN  ///< Two humans on the same machine.
    };

    /** @brief Strength level of the computer opponent. */
    enum class Difficulty { EASY, MEDIUM, HARD };

    /** @brief Configuration assembled by the menu. */
    struct GameSettings {
        /** @brief Game mode. */
        GameMode mode = GameMode::HUMAN_VS_AI;

        /** @brief Engine level, ignored in human versus human. */
        Difficulty difficulty = Difficulty::MEDIUM;

        /** @brief Name chosen by the first player, possibly empty. */
        std::string player_name;

        /**
         * @brief Opponent name, when known.
         * @note Empty for a new match, where it is derived from the mode; populated
         * when loading a save, which carries the name with it.
         */
        std::string opponent_name;

        /** @brief Board side length. */
        int board_size = 11;

        /**
         * @brief Colour played by the human who configured the match.
         * @note Red moves first, so choosing Blue concedes the opening. Ignored
         * between two humans, where both colours are human.
         */
        hex::Player human_colour = hex::Player::RED;

        /** @brief Clock budget per player, in seconds. */
        double seconds_per_player = hexplay::DEFAULT_CLOCK_SECONDS;

        /**
         * @brief Arcade mode: black holes on the board and a Blitz clock.
         * @note One switch rather than two, because the variants are a single offer
         * to the player. Splitting them would put four combinations in the menu to
         * serve the one anybody actually picks.
         */
        bool arcade = false;
    };

    /**
     * @brief Builds the clock these settings call for.
     *
     * Lives here rather than in the game screen because the time budget follows
     * from the menu choices, not from presentation.
     *
     * @note Arcade overrides the per-match budget outright: Blitz is ten seconds
     * per move and does not compose with an overall limit.
     */
    [[nodiscard]] hexplay::GameClock clockFor(const GameSettings& s);

    /**
     * @brief Maps a difficulty level to a search configuration.
     *
     * Difficulty controls thinking time, bridge defence and the number of trees
     * used by root parallelisation. Bridge defence is worth several points of
     * strength at equal time, so it is what separates the top level from the other
     * two, which differ only in search budget. Lower levels stay at a single tree
     * so their strength does not depend on the host's core count.
     */
    [[nodiscard]] hex::MCTSConfig configFor(Difficulty d);

    /**
     * @brief Returns the string key describing a game mode.
     * @note A key rather than the text itself: the UI knows the selected language
     * and this translation unit does not, and duplicating the translation table
     * here would let the two copies drift.
     */
    [[nodiscard]] hexui::StringKey labelOf(GameMode m);

    /** @brief Returns the string key describing a difficulty level. */
    [[nodiscard]] hexui::StringKey labelOf(Difficulty d);

    /**
     * @brief Returns the same settings with the roles swapped, for a rematch.
     *
     * Hex is not symmetric: Red moves first and the first-move advantage is real,
     * so replaying with the same colours hands the same player the same edge twice.
     * Alternating is how the game evens out, and the reason the pie rule exists.
     *
     * What gets swapped depends on who is playing, not on how the match ended:
     * against the engine the human's colour flips and the names follow, while
     * between two humans the names are swapped, since there the first name is Red
     * by convention and there is no human_colour to flip.
     *
     * @note Pure function: the next match is built from these settings exactly as
     * it would be from the menu, with no shared state to reset.
     */
    [[nodiscard]] GameSettings rematchOf(const GameSettings& s);

    /**
     * @brief Tests whether the given colour is played by a human.
     * @note True for both colours between two humans, and only for the selected
     * colour against the engine.
     */
    [[nodiscard]] bool isHumanColour(const GameSettings& s, hex::Player p);

    /**
     * @brief Returns the name of the player holding the given colour.
     * @param s Match settings.
     * @param p Colour to resolve.
     * @param loc Supplies the localized fallback names, which are interface text
     * like everything else.
     */
    [[nodiscard]] std::string nameOfColour(const GameSettings& s, hex::Player p,
                                           const hexui::LocalizationManager& loc);

    /**
     * @brief Returns the first player's name, falling back to a localized default
     * so a blank nickname never blocks a match.
     */
    [[nodiscard]] std::string firstPlayerName(const GameSettings& s,
                                              const hexui::LocalizationManager& loc);

    /** @brief Returns the opponent's name: the second human or the engine level. */
    [[nodiscard]] std::string secondPlayerName(const GameSettings& s,
                                               const hexui::LocalizationManager& loc);
}

#endif //GAME_SETTINGS_H
