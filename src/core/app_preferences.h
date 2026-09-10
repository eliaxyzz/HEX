/**
 * @file app_preferences.h
 * @brief Application preferences, persisted across runs.
 *
 * Distinct from the match settings in game_settings.h, which die with the match:
 * these are choices about the application itself and must survive shutdown.
 *
 * Text format, one preference per line, comments allowed:
 *
 *     # HEX - preferences
 *     fullscreen=0
 *     muted=0
 *     music_volume=45
 *
 * @note The file is untrusted input. A hand-edited, truncated or
 * newer-than-expected file must not block startup, so any line that cannot be
 * understood is ignored and the corresponding preference keeps its default:
 * losing a preference is an annoyance, failing to start is a fault.
 * @note SFML-free: parsing and serialisation are pure functions, covered by the
 * test suite like the save format.
 */

#ifndef APP_PREFERENCES_H
#define APP_PREFERENCES_H

#include <string>

#include "core/localization.h"
#include "core/player_profile.h"

namespace hexapp {

    /** @brief User choices that outlive the process. */
    struct Preferences {
        /** @brief Whether the window opens fullscreen. */
        bool fullscreen = false;

        /** @brief Whether sound effects are muted. */
        bool muted = false;

        /**
         * @brief Whether stones carry a distinguishing geometric marker.
         * @note A preference rather than a match setting: it describes the person at
         * the screen, not how the game is played, so it must not be re-selected on
         * every launch.
         */
        bool colorblind = false;

        /**
         * @brief Board colour scheme.
         *
         * A preference rather than profile data because it is a choice, not an
         * achievement: the profile records which palettes are unlocked and cannot be
         * lowered by playing less, while this records which one is in use and
         * changes freely. Deleting settings.ini therefore restores the classic
         * scheme without revoking anything earned.
         *
         * @note A palette that is no longer unlocked is not honoured; usablePalette()
         * falls back to the classic one.
         */
        BoardPalette palette = BoardPalette::CLASSIC;

        /**
         * @brief Background music volume, 0 to 100.
         * @note Separate from the mute flag, which covers everything: move feedback
         * is information and can be kept while the atmosphere is switched off. At 0
         * the music never starts.
         */
        int music_volume = 45;

        /**
         * @brief Matches won and lost by the human against the engine.
         * @note Kept with the preferences because they share a lifetime: they
         * outlive the process and belong to no particular match. Local human versus
         * human games are excluded, since a win there has no owner, and so are
         * online games, which do not pass through this screen.
         */
        int wins = 0;
        int losses = 0;

        /**
         * @brief Interface language.
         * @note A file written before this key existed simply lacks it and yields
         * Italian, which is both the default and what that build displayed.
         */
        hexui::Language language = hexui::Language::IT;

        bool operator==(const Preferences&) const = default;
    };

    /** @brief Name of the file holding the preferences. */
    inline constexpr const char* PREFERENCES_FILE = "settings.ini";

    /** @brief Longest accepted line, in bytes. */
    inline constexpr std::size_t MAX_PREFERENCE_LINE = 512;

    /** @brief Serialises the preferences to the file format. */
    [[nodiscard]] std::string serializePreferences(const Preferences& prefs);

    /**
     * @brief Parses the file format.
     *
     * Blank lines, comments (# or ;), unknown keys, unreadable values and
     * over-long lines are dropped silently; whatever cannot be read keeps its
     * default.
     *
     * Accepted as true: 1, true, on, si, yes. Accepted as false: 0, false, off,
     * no. Comparison ignores case and surrounding whitespace.
     *
     * Counters accept non-negative integers only, so an unreadable or negative
     * value leaves the counter at zero instead of propagating a nonsensical figure
     * to the menu screen.
     */
    [[nodiscard]] Preferences parsePreferences(const std::string& text);

    /**
     * @brief Loads the preferences from the given file.
     * @note A missing or unreadable file is not an error: the defaults are
     * returned, which is exactly a first-run configuration.
     */
    [[nodiscard]] Preferences loadPreferences(const std::string& path = PREFERENCES_FILE);

    /**
     * @brief Writes the preferences to the given file.
     * @return true if the write succeeded.
     */
    [[nodiscard]] bool savePreferences(const Preferences& prefs,
                                       const std::string& path = PREFERENCES_FILE);
}

#endif //APP_PREFERENCES_H
