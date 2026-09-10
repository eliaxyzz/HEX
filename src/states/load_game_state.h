/**
 * @file load_game_state.h
 * @brief Save slot selection screen.
 *
 * With one file per name, no shortcut can guess which save "Load game" should
 * open, so the choice belongs to the user and gets a screen of its own.
 *
 * ### It selects, it does not load
 *
 * The loading itself stays what it always was — readFromFile, materialise, moves
 * into the context, transition to the match — because that is the same verified
 * path the menu already used. This screen adds only the missing step: saying which
 * match.
 *
 * ### A list, and the two operations that belong to it
 *
 * Select, rename, delete. No disk browsing and no moving files: the operating
 * system already does that better, on a folder the user can open. Rename and
 * delete cannot be left out, though: a save's name is the only thing telling one
 * match from another in this list, and a folder that grows with no way to prune it
 * stops being useful after a month of play.
 *
 * @note Both go through a modal, for the same reason: they are the only two
 * actions on this screen the user cannot undo by simply going back.
 */

#ifndef LOAD_GAME_STATE_H
#define LOAD_GAME_STATE_H

#include <string>
#include <vector>

#include "states/confirm_dialog.h"
#include "states/sfml_app_state.h"
#include "ui/ui_widgets.h"

namespace hexapp {

    /** @brief Saves shown on one page. */
    inline constexpr std::size_t SAVES_PER_PAGE = 6;

    /** @brief List of the available saves, with selection or cancellation. */
    class LoadGameState final : public SfmlAppState {
    public:
        /** @brief Builds the screen by reading the saves directory. */
        explicit LoadGameState(AppContext& context);

        [[nodiscard]] StateId id() const override { return StateId::LOAD_GAME; }

        void handleInput(const InputEvent& event) override;
        void update(float dt) override;
        void draw(sf::RenderTarget& target) override;

    private:
        /** @brief Shared application context. Non-owning. */
        AppContext& context;

        /** @brief Save names, in alphabetical order. */
        std::vector<std::string> saves;

        /** @brief Index of the first save shown on the current page. */
        std::size_t first = 0;

        /**
         * @brief One button per visible row, reused across pages.
         * @note Always SAVES_PER_PAGE of them; those without a matching save are
         * neither drawn nor consulted. Rebuilding them per page would add nothing but
         * an allocation.
         */
        std::vector<hexui::Button> entries;

        /**
         * @brief Rename and delete, one pair per visible row.
         * @note Placed beside the name rather than on a separate screen because they
         * act on that row: a command operating on an item belongs where the item is,
         * or it still has to explain what it operates on.
         */
        std::vector<hexui::Button> renames;
        std::vector<hexui::Button> deletes;

        /** @brief Previous and next page; present only when needed. */
        hexui::Button prev;
        hexui::Button next;

        /** @brief Returns to the menu without loading anything. */
        hexui::Button back;

        /** @brief Error message from the last load attempt. */
        std::string message;

        // --- Management of the selected save ---------------------------------

        /**
         * @brief The save the open modal is working on.
         * @note A row index would not do: a row is a position on the page, and the
         * page can change underneath between opening the modal and confirming, a
         * resize that re-lays out being enough. The name identifies the file, which
         * is what both operations must touch.
         */
        std::string target_slot;

        /** @brief The question asked before deleting; modal, consumes all input. */
        ConfirmDialog delete_dialog;

        /** @brief True while the rename prompt is open. */
        bool rename_prompt = false;

        /** @brief The new name, prefilled with the current one. */
        hexui::TextInput rename_name;

        /** @brief The two answers of the rename prompt. */
        hexui::Button rename_confirm;
        hexui::Button rename_cancel;

        /** @brief Returns how many rows the current page actually fills. */
        [[nodiscard]] std::size_t visibleCount() const;

        /** @brief Test whether a page exists before or after the current one. */
        [[nodiscard]] bool hasPrev() const { return first > 0; }
        [[nodiscard]] bool hasNext() const { return first + SAVES_PER_PAGE < saves.size(); }

        /** @brief Recomputes widget placement for the current view size. */
        void layout();

        /**
         * @brief Loads the given save and starts the rebuilt match.
         * @note A malformed or tampered file produces a message and no transition:
         * the screen stays put, where another file can be chosen.
         */
        void loadSave(const std::string& slot);

        /**
         * @brief Re-reads the directory and puts page and widgets back in order.
         * @note After a deletion the current page can sit past the end of the list, so
         * `first` is pulled back to the last page that really exists; otherwise the
         * user would face an empty page with a non-empty list.
         */
        void refresh();

        /** @brief Opens the delete confirmation for the given save. */
        void openDeletePrompt(const std::string& slot);

        /** @brief Deletes target_slot and refreshes the list. */
        void confirmDelete();

        /** @brief Opens the rename prompt with the field prefilled. */
        void openRenamePrompt(const std::string& slot);

        /** @brief Closes the rename prompt without touching anything. */
        void closeRenamePrompt();

        /**
         * @brief Renames target_slot to the field's contents.
         * @note The name goes through the same normalisation as saving, so anything
         * that is not a valid name is rejected here rather than by the filesystem.
         */
        void confirmRename();

        /**
         * @brief Routes an event into the rename prompt, if it is open.
         * @return true if the modal consumed the event.
         */
        bool handleRenameInput(const InputEvent& event);

        /** @brief Draws the rename veil and panel. */
        void drawRenamePrompt(sf::RenderTarget& target) const;
    };
}

#endif //LOAD_GAME_STATE_H
