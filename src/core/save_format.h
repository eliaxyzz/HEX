/**
 * @file save_format.h
 * @brief Saved game file format.
 *
 * Text, one field per line, version-stamped: readable by eye and diffable, which
 * for a project of this size is worth more than compactness.
 *
 *     hexsave 2
 *     size 11
 *     mode human_vs_ai
 *     difficulty hard
 *     human blue
 *     red Computer Hard
 *     blue Player
 *     moves E5 PIE C3 F7
 *
 * An Arcade game adds a line listing the walled cells, in the same notation as the
 * moves, and bumps the version, since without that line the file would describe a
 * different game:
 *
 *     hexsave 3
 *     ...
 *     holes B1 J1 I9 D11
 *     moves E5 C3 F7
 *
 * The `mode` and `human` lines record who was playing, not just what was played.
 * Without the latter, a reloaded game knows which stones are on the board but not
 * who was moving them, and a rematch could not tell which roles to swap.
 *
 * One file per saved game, named `saves/[name].hex` with the name chosen by the
 * player. There is deliberately no single slot: silently overwriting the only
 * saved game is data loss nobody asked for.
 *
 * @note The file is untrusted input even when the game wrote it, since it may have
 * been hand-edited or truncated. Parsing bounds everything - line length, board
 * size, move count, name length - and materialise() rejects any move that is not
 * legal in the position it would appear in.
 * @note SFML-free: serialisation, parsing and validation are pure functions
 * covered by the test suite.
 */

#ifndef SAVE_FORMAT_H
#define SAVE_FORMAT_H

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/game.h"
#include "core/game_settings.h"
#include "core/move.h"

namespace hexsave {

    /**
     * @brief Format version stamped at the head of the file.
     * @note Version 2 introduced the `human` line. Version 1 files stay readable:
     * they do not record which colour the human held, and Red is assumed, matching
     * what the game assigned when those files were written.
     */
    inline constexpr int FORMAT_VERSION = 3;

    /**
     * @brief Lowest version able to express walled cells.
     * @note The stamped number declares which features the file requires, not which
     * build wrote it. A normal game therefore stays at version 2 and remains
     * readable by already released builds, and only an Arcade game reaches 3.
     * Bumping every save for a line almost none of them carry would have broken
     * backward compatibility for content that did not change.
     */
    inline constexpr int HOLES_FORMAT_VERSION = 3;

    /** @brief Oldest version still readable. */
    inline constexpr int MIN_FORMAT_VERSION = 1;

    /** @brief Largest accepted side: past Z the columns run out of letters. */
    inline constexpr int MAX_BOARD_SIZE = 26;

    /** @brief Smallest accepted side. */
    inline constexpr int MIN_BOARD_SIZE = 2;

    /** @brief Longest accepted file line, in bytes. */
    inline constexpr std::size_t MAX_LINE_LENGTH = 4096;

    /** @brief Longest accepted player name, in bytes. */
    inline constexpr std::size_t MAX_NAME_LENGTH = 64;

    /** @brief Move category as written in the file. */
    enum class TokenKind { ADD, PIE, RESIGN };

    /**
     * @brief A move as it appears in the file.
     * @note A placement records the position only; the colour is derived from the
     * turn. A tampered file therefore cannot play a stone of the wrong colour,
     * because that information is not the file's to give.
     */
    struct MoveToken {
        TokenKind kind = TokenKind::ADD;
        int row = 0;
        int col = 0;

        bool operator==(const MoveToken&) const = default;
    };

    /** @brief Decoded contents of a save file. */
    struct SaveData {
        int board_size = 11;
        hexapp::GameMode mode = hexapp::GameMode::HUMAN_VS_AI;
        hexapp::Difficulty difficulty = hexapp::Difficulty::MEDIUM;

        /**
         * @brief Colour held by the human who configured the match.
         * @note Defaults to Red, which is what version 1 files imply. Meaningless
         * between two humans, exactly as in the settings it comes from.
         */
        hex::Player human_colour = hex::Player::RED;

        std::string red_name;
        std::string blue_name;
        std::vector<MoveToken> moves;

        /**
         * @brief Arcade black holes, in cell notation.
         *
         * Empty for a normal game, which is exactly what distinguishes one: the
         * holes are the only part of Arcade the file must remember, since Blitz is a
         * clock rule rebuilt from the settings.
         *
         * @note Reapplied before the moves are replayed, so a tampered file placing
         * a stone inside a hole is rejected by the same validation that rejects
         * every other illegal move, rather than by a special-purpose check.
         */
        std::vector<std::pair<int, int>> holes;
    };

    // --- Move notation ------------------------------------------------------

    /** @brief Formats a move token as written in the file: "E5", "PIE", "RESIGN". */
    [[nodiscard]] std::string tokenToString(const MoveToken& token);

    /** @brief Parses a file token, if well formed and inside the board. */
    [[nodiscard]] std::optional<MoveToken> tokenFromString(const std::string& text, int board_size);

    /** @brief Converts played moves into the tokens to be written. */
    [[nodiscard]] std::vector<MoveToken> tokensFromMoves(const std::vector<hex::Move>& moves);

    // --- Serialisation ------------------------------------------------------

    /** @brief Serialises the save data to the file format. */
    [[nodiscard]] std::string serialize(const SaveData& data);

    /**
     * @brief Parses a save file.
     * @param text File contents.
     * @param error Filled with the rejection reason when parsing fails.
     */
    [[nodiscard]] std::optional<SaveData> parse(const std::string& text, std::string& error);

    /**
     * @brief Resolves a single token into the move it denotes.
     *
     * This is where the colour is taken from the turn and where a swap finds the
     * stone to recolour: the token carries neither, so a sender can lie about
     * neither. Shared by save loading and network reception, two untrusted sources
     * that must read tokens identically.
     *
     * @param situation Position the token is interpreted in.
     * @param token Token to resolve.
     * @param error Filled with the rejection reason.
     * @note The move is not yet validated by the engine; callers must still put it
     * through Situation::isValid().
     */
    [[nodiscard]] std::optional<hex::Move> moveFromToken(const hex::Situation& situation,
                                                         const MoveToken& token,
                                                         std::string& error);

    /**
     * @brief Resolves the tokens into real moves by replaying them on a board.
     *
     * Every step is validated: a token that does not denote a legal move fails the
     * whole conversion rather than part of it.
     *
     * @param data Already parsed file contents.
     * @param error Filled with the rejection reason.
     */
    [[nodiscard]] std::optional<std::vector<hex::Move>> materialise(const SaveData& data,
                                                                    std::string& error);

    /**
     * @brief Builds the settings a saved match should be reopened with.
     * @note The file stores names by colour (red, blue) while the settings store
     * them by role (whoever configured the match, and their opponent). This
     * function performs that conversion through human_colour; without it, loading a
     * game played as Blue would put the engine's name on the human and a rematch
     * would swap two already swapped names.
     */
    [[nodiscard]] hexapp::GameSettings settingsFrom(const SaveData& data);

    // --- Files --------------------------------------------------------------

    /** @brief Longest accepted slot name, in characters. */
    inline constexpr std::size_t MAX_SLOT_LENGTH = 32;

    /**
     * @brief Tests whether a slot name may become a file inside saves/.
     * @note The name is user input and therefore untrusted like the file contents:
     * letters, digits, space, hyphen and underscore are accepted and nothing else.
     * Path separators and dots cannot appear, so no name can escape the directory.
     */
    [[nodiscard]] bool isValidSlotName(const std::string& slot);

    /**
     * @brief Normalises a user-supplied slot name.
     * @return The usable name, or nullopt if nothing valid remains.
     */
    [[nodiscard]] std::optional<std::string> normaliseSlotName(const std::string& slot);

    /** @brief Returns the file path of the given save slot. */
    [[nodiscard]] std::string savePath(const std::string& slot);

    /** @brief Tests whether a save with that name exists. */
    [[nodiscard]] bool saveExists(const std::string& slot);

    /**
     * @brief Lists the saves in the directory, alphabetically.
     * @return Slot names rather than paths, since that is what the screen shows and
     * what readFromFile() accepts. A missing or unreadable directory is not an
     * error, it is an empty list.
     */
    [[nodiscard]] std::vector<std::string> listSaves();

    /**
     * @brief Writes the save to disk, creating the directory if needed.
     * @return true if the write succeeded.
     */
    [[nodiscard]] bool writeToFile(const std::string& slot, const SaveData& data, std::string& error);

    /**
     * @brief Reads and parses a save file.
     * @param slot Slot name.
     * @param error Filled with the failure reason.
     */
    [[nodiscard]] std::optional<SaveData> readFromFile(const std::string& slot, std::string& error);

    /**
     * @brief Deletes the save with the given name.
     *
     * The name is validated before the disk is touched, exactly as on write:
     * deletion is irreversible, and something that is not a valid name must never
     * reach the filesystem.
     *
     * @param slot Slot name.
     * @param error Filled with the failure reason.
     * @return true if the file no longer exists on return.
     */
    [[nodiscard]] bool deleteSave(const std::string& slot, std::string& error);

    /**
     * @brief Renames a save.
     *
     * Refuses to overwrite an existing save: two games sharing a name are
     * indistinguishable on screen, and picking one on the user's behalf would
     * silently discard the other.
     *
     * @param from Current name.
     * @param to New name, already normalised by the caller.
     * @param error Filled with the failure reason.
     * @return true if the save is named `to` on return.
     */
    [[nodiscard]] bool renameSave(const std::string& from, const std::string& to,
                                  std::string& error);
}

#endif //SAVE_FORMAT_H
