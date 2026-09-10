/**
 * @file localization.h
 * @brief Interface strings in Italian and English.
 *
 * Keys are enumerators rather than strings. A dictionary indexed by string
 * ("menu.play") fails at run time: a mistyped key simply does not exist, and the
 * defect surfaces only when someone opens that screen. Here an unknown key fails
 * to compile, and a key left untranslated is caught by the test walking the whole
 * table.
 *
 * Both languages sit on the same table row instead of in two parallel arrays.
 * Separate lists drift apart on the first insertion in the middle, after which
 * one language silently answers with the wrong phrase.
 *
 * @note Not everything is translated: the console executable and the diagnostics
 * raised by the engine and the save format stay in one language, as they target
 * developers rather than players. Where such a message reaches the screen, the
 * introductory part is localized and the detail is not.
 * @note SFML-free: a table and a lookup, verifiable without opening a window.
 */

#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace hexui {

    /** @brief Supported languages; Italian is the default. */
    enum class Language { IT, EN };

    /**
     * @brief Identifier of an interface phrase.
     * @note The order carries no meaning but must match the translation table row
     * for row, which the test suite verifies.
     */
    enum class StringKey : std::size_t {
        // --- Shared ---
        COLOUR_RED, COLOUR_BLUE,
        COLOUR_RED_UPPER, COLOUR_BLUE_UPPER,
        BACK,

        // --- Default player names ---
        NAME_HUMAN, NAME_HUMAN_2, NAME_COMPUTER, NAME_GUEST,

        // --- Main menu ---
        MENU_SUBTITLE,
        MENU_MODE_HUMAN_COMPUTER, MENU_MODE_HUMAN_HUMAN,
        MENU_VARIANT_NORMAL, MENU_VARIANT_ARCADE,
        MENU_DIFFICULTY_EASY, MENU_DIFFICULTY_MEDIUM, MENU_DIFFICULTY_HARD,
        MENU_NAME_1, MENU_NAME_2,
        MENU_PLAY_AS_RED, MENU_PLAY_AS_BLUE, MENU_PLAY_AS_RANDOM,
        MENU_PLAY, MENU_LOAD, MENU_ONLINE, MENU_SETTINGS, MENU_QUIT,
        MENU_LOAD_TITLE, MENU_LOAD_EMPTY, MENU_LOAD_HINT, MENU_LOAD_PREV, MENU_LOAD_NEXT,

        // --- Save slot management ---
        SAVE_DELETE_PROMPT, SAVE_DELETE_CONFIRM, SAVE_DELETE_FAILED,
        SAVE_RENAME_PROMPT, SAVE_RENAME_CONFIRM, SAVE_RENAME_FAILED,
        SAVE_MANAGE_HINT,
        MENU_HINT,
        MENU_WINS, MENU_LOSSES, MENU_LEVEL,
        RANK_ROOKIE, RANK_HACKER, RANK_MASTERMIND, RANK_LEGEND,
        MENU_LOAD_FAILED, MENU_SAVE_INVALID,

        // --- Local match ---
        GAME_PIE_RULE, GAME_NEW_GAME, GAME_SAVE,
        GAME_LEAVE, GAME_LEAVE_PROMPT, GAME_LEAVE_HINT, GAME_MENU_HINT_OVER,
        GAME_MENU_HINT,
        GAME_THINKING,
        GAME_SAVED, GAME_MOVES, GAME_SAVE_FAILED, GAME_REPLAY_FAILED,
        GAME_SAVE_PROMPT, GAME_SAVE_NAME, GAME_SAVE_CONFIRM, GAME_CANCEL,
        GAME_SAVE_BAD_NAME, GAME_SAVE_HINT,
        ARCADE_BLITZ,

        // --- Match status ---
        STATUS_TO_MOVE, STATUS_TURN_OF, STATUS_WON,
        NOTICE_PIE_SWAPPED, NOTICE_UNDONE, NOTICE_UNDONE_END,
        NOTICE_ILLEGAL_MOVE, NOTICE_TIMEOUT, NOTICE_PLAYER_ERROR,

        // --- End reasons ---
        REASON_CONNECTION, REASON_RESIGN, REASON_TIMEOUT, REASON_ILLEGAL, REASON_UNKNOWN,

        // --- Settings ---
        SETTINGS_TITLE, SETTINGS_SUBTITLE,
        SETTINGS_WINDOWED, SETTINGS_FULLSCREEN,
        SETTINGS_AUDIO_ON, SETTINGS_MUTED,
        SETTINGS_LABEL_DISPLAY, SETTINGS_LABEL_SFX, SETTINGS_LABEL_LANGUAGE,
        SETTINGS_LABEL_COLOURS,
        SETTINGS_LANGUAGE_IT, SETTINGS_LANGUAGE_EN,
        SETTINGS_COLOURS_STANDARD, SETTINGS_COLOURS_COLORBLIND,
        SETTINGS_LABEL_PALETTE,
        PALETTE_CLASSIC, PALETTE_TOXIC, PALETTE_PRESTIGE,
        PALETTE_REQUIRES_LEVEL,
        SETTINGS_MUSIC, SETTINGS_MUSIC_OFF, SETTINGS_MUSIC_LOW,
        SETTINGS_MUSIC_MEDIUM, SETTINGS_MUSIC_HIGH,
        MENU_TUTORIAL,
        TUTORIAL_TITLE, TUTORIAL_STEP_GOAL, TUTORIAL_STEP_CHAIN,
        TUTORIAL_STEP_ALMOST, TUTORIAL_DONE, TUTORIAL_EXIT,
        TUTORIAL_TITLE_BLUE, TUTORIAL_BLUE_GOAL, TUTORIAL_BLUE_CHAIN,
        TUTORIAL_BLUE_ALMOST, TUTORIAL_BLUE_DONE, TUTORIAL_NEXT,
        TUTORIAL_TITLE_PIE, TUTORIAL_PIE_BODY, TUTORIAL_NEXT_PIE,
        TUTORIAL_TITLE_ARCADE, TUTORIAL_ARCADE_BODY, TUTORIAL_NEXT_ARCADE,
        SETTINGS_WRITE_FAILED,

        // --- Network lobby ---
        LOBBY_TITLE, LOBBY_HINT,
        LOBBY_ADDRESS, LOBBY_NAME, LOBBY_CONNECT,
        LOBBY_CONNECTING, LOBBY_WAITING,
        LOBBY_CONNECT_FAILED, LOBBY_SERVER_CLOSED, LOBBY_CONNECTION_LOST,
        LOBBY_BAD_START, LOBBY_REFUSED, LOBBY_OPPONENT_LEFT,

        // --- Online match ---
        NET_RESIGN, NET_LEAVE, NET_LEAVE_HINT,
        NET_OPPONENT, NET_STILL_CONNECTED,
        NET_YOUR_TURN, NET_WAIT_FOR, NET_YOU_WON, NET_HAS_WON,
        NET_BAD_STATE, NET_UNREADABLE_STATE,

        /** @brief Sentinel holding the key count; not a phrase. */
        COUNT
    };

    /** @brief Number of translated phrases. */
    inline constexpr std::size_t STRING_COUNT = static_cast<std::size_t>(StringKey::COUNT);

    /**
     * @brief Serves the interface phrases in the selected language.
     *
     * Both languages are built once and one is kept active, so switching costs
     * nothing and allocates nothing. That is what lets the menu refresh in the same
     * frame as the click.
     */
    class LocalizationManager {
    public:
        /** @brief Builds the manager in the given language. */
        explicit LocalizationManager(Language language = Language::IT);

        /** @brief Returns the active language. */
        [[nodiscard]] Language language() const { return active; }

        /** @brief Switches language; subsequent lookups use the new one. */
        void setLanguage(Language language) { active = language; }

        /**
         * @brief Returns the phrase for the given key in the active language.
         * @note The reference stays valid for the lifetime of the manager, language
         * switches included, since both tables exist at all times.
         */
        [[nodiscard]] const std::string& text(StringKey key) const;

        /** @brief Returns the two-letter language code used in the preferences file. */
        [[nodiscard]] static std::string_view codeOf(Language language);

        /**
         * @brief Parses a language code.
         * @return nullopt when the code matches no known language, so a preferences
         * reader can fall back to the default.
         */
        [[nodiscard]] static std::optional<Language> languageFromCode(std::string_view code);

        /** @brief Returns a phrase in an explicit language, ignoring the active one. */
        [[nodiscard]] static const std::string& textIn(Language language, StringKey key);

    private:
        Language active;
    };
}

#endif //LOCALIZATION_H
