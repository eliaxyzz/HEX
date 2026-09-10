/**
 * @file player_profile.h
 * @brief Player progression across matches: experience and level.
 *
 * The win and loss counters record how many matches were played but not what they
 * were worth: a win against Hard and a first-turn loss each add one row.
 * Experience records how much has been played, and the level summarises it in a
 * single figure.
 *
 * Stored in its own file rather than as extra preference keys, because the two
 * have different lifetimes. Preferences are choices, resettable without losing
 * anything earned; the profile is what the player accumulated, and deleting
 * settings.ini to fix a window problem must not cost a level. It therefore lives
 * in saves/, next to the games, which is the player-data directory rather than
 * the configuration one.
 *
 * The format matches the preferences file for the same reason: readable text that
 * degrades to defaults when corrupt instead of blocking startup.
 *
 *     # HEX - player profile
 *     xp=1250
 *
 * @note SFML-free: parsing, serialisation and the level computation are pure
 * functions covered by the test suite.
 */

#ifndef PLAYER_PROFILE_H
#define PLAYER_PROFILE_H

#include <string>

#include "core/localization.h"

namespace hexapp {

    /** @brief Experience granted for a human win. */
    inline constexpr int XP_FOR_WIN = 100;

    /**
     * @brief Experience granted for a human loss, timeouts included.
     * @note Deliberately non-zero: a lost match is still a match played, and a
     * system rewarding only wins stalls exactly the players who most need a reason
     * to keep playing.
     */
    inline constexpr int XP_FOR_LOSS = 25;

    /** @brief Experience required per level beyond the first. */
    inline constexpr int XP_PER_LEVEL = 500;

    /**
     * @brief Ceiling on accumulated experience.
     * @note Serves two purposes: stopping a tampered file from declaring a value
     * that would overflow the sums, and giving the level an explicit maximum rather
     * than one implied by the integer type.
     */
    inline constexpr int MAX_XP = 100'000'000;

    /** @brief Default path of the profile file. */
    inline constexpr const char* DEFAULT_PROFILE_PATH = "saves/profile.ini";

    /**
     * @brief Board appearance, unlocked by levelling up.
     *
     * Cosmetic rewards only: they change colours and nothing else. None affects the
     * rules, the engine strength or what a player may do, deliberately so, since a
     * progression that unlocks advantages turns every new player into a handicapped
     * opponent.
     *
     * The enum lives here rather than with the theme because it answers "what has
     * been earned", not "what colour is a stone". The concrete palettes belong to
     * sfml_theme.h, the one place where colours are colours, which keeps the unlock
     * rule verifiable without opening a window.
     */
    enum class BoardPalette {
        CLASSIC,   ///< The usual red and blue. Available from the start.
        TOXIC,     ///< Neon green against purple. Level 2.
        PRESTIGE   ///< Gold against silver. Level 3.
    };

    /** @brief Returns the level required to use the given palette. */
    [[nodiscard]] int requiredLevelFor(BoardPalette palette);

    /** @brief Tests whether the given experience unlocks the palette. */
    [[nodiscard]] bool isPaletteUnlocked(BoardPalette palette, int xp);

    /**
     * @brief Resolves the palette to actually use, given the profile.
     *
     * An illegitimate choice, from a hand-edited settings.ini or a profile reset
     * after picking an advanced palette, falls back to the classic scheme instead of
     * being honoured. The check lives here rather than in the settings screen
     * because that screen is only where a choice is made: what counts as valid must
     * be decided in a single place, and this is the one every startup path already
     * goes through.
     */
    [[nodiscard]] BoardPalette usablePalette(BoardPalette chosen, int xp);

    /** @brief What the player carries between matches. */
    struct Profile {
        /** @brief Total experience accumulated; never decreases. */
        int xp = 0;

        bool operator==(const Profile&) const = default;
    };

    /**
     * @brief Returns the level for the given experience, starting at 1.
     * @note Level 1 spans 0 to XP_PER_LEVEL - 1, so a player who has never played is
     * level 1 rather than level 0: it is a rank, not a count.
     */
    [[nodiscard]] int levelFor(int xp);

    /** @brief Returns the experience accumulated within the current level. */
    [[nodiscard]] int xpIntoLevel(int xp);

    /**
     * @brief Returns the rank title for a level, as a translation key.
     * @note A key rather than a phrase, since the title is shown in the menu and
     * follows the language like everything else. The title stops changing from level
     * four on: past the final rank there is nothing the game could promise and keep.
     */
    [[nodiscard]] hexui::StringKey titleKeyFor(int level);

    /**
     * @brief Adds experience, saturating at MAX_XP.
     * @return Updated profile; the argument is left untouched.
     */
    [[nodiscard]] Profile withXpGain(const Profile& profile, int gain);

    // --- Persistence ---------------------------------------------------------

    /** @brief Serialises the profile to the file format. */
    [[nodiscard]] std::string serializeProfile(const Profile& profile);

    /**
     * @brief Parses a profile file.
     * @note An unreadable line keeps the default, so a corrupt profile restarts the
     * progression instead of blocking play.
     */
    [[nodiscard]] Profile parseProfile(const std::string& text);

    /** @brief Loads the profile from disk; a missing file yields a fresh one. */
    [[nodiscard]] Profile loadProfile(const std::string& path = DEFAULT_PROFILE_PATH);

    /**
     * @brief Writes the profile to disk, creating the directory if needed.
     * @return true if the write succeeded.
     */
    [[nodiscard]] bool saveProfile(const Profile& profile,
                                   const std::string& path = DEFAULT_PROFILE_PATH);
}

#endif //PLAYER_PROFILE_H
