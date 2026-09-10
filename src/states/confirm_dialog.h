/**
 * @file confirm_dialog.h
 * @brief Modal confirmation dialog, shared by the game screens.
 *
 * One question and two answers over a veil that dims what lies beneath. Used
 * wherever a stray click would be expensive, such as leaving a running match,
 * locally or online.
 *
 * One class rather than two copies: the local and online screens differ for good
 * reasons, one holds a GameController and the other a connection, but "do you
 * really want to leave?" is the same question and must look and answer the same in
 * both. Duplicating it would have worked today and started diverging at the first
 * fix applied to only one copy.
 *
 * @note Genuinely modal: while open it consumes every input event, and handleInput
 * tells the caller whether the event was used so the screen below never sees it.
 * Otherwise Escape would exit while the user is answering the question about
 * exiting, and a click would fall through the panel onto the board.
 */

#ifndef CONFIRM_DIALOG_H
#define CONFIRM_DIALOG_H

#include <SFML/Graphics.hpp>

#include "core/localization.h"
#include "states/sfml_app_state.h"
#include "ui/ui_icons.h"
#include "ui/ui_widgets.h"

namespace hexapp {

    /** @brief What became of an event handed to the dialog. */
    enum class DialogOutcome {
        IGNORED,    ///< Dialog closed; the event belongs to the screen.
        CONSUMED,   ///< Used by the dialog, without deciding anything.
        CONFIRMED,  ///< User confirmed; the dialog closed.
        CANCELLED   ///< User cancelled; the dialog closed.
    };

    /** @brief A modal question offering confirm and cancel. */
    class ConfirmDialog {
    public:
        /**
         * @brief Builds the dialog with the strings it presents.
         * @param context Shared context: size, theme, fonts, audio, language.
         * @param prompt Key of the question.
         * @param hint Key of the line explaining the two keys.
         * @param confirm_label Key of the affirmative answer's label.
         * @param confirm_icon Icon beside the affirmative answer. Conveys the kind of
         * consequence before the label is read: a door for leaving a match, a bin for
         * deleting a file.
         */
        ConfirmDialog(AppContext& context, hexui::StringKey prompt, hexui::StringKey hint,
                      hexui::StringKey confirm_label,
                      hexgui::Icon confirm_icon = hexgui::Icon::EXIT);

        /** @brief Opens the dialog. */
        void open() { is_open = true; }

        /** @brief Closes the dialog without deciding anything. */
        void close() { is_open = false; }

        /** @brief Tests whether the dialog is open. */
        [[nodiscard]] bool isOpen() const { return is_open; }

        /** @brief Recentres the panel within the current view size. */
        void layout();

        /**
         * @brief Routes an event into the dialog.
         *
         * @param event Event to route.
         * @return What became of the event.
         * @note Resize is the one event that is not consumed: it concerns the whole
         * screen rather than the panel, and the caller must be able to re-lay out the
         * rest before calling layout().
         */
        DialogOutcome handleInput(const InputEvent& event);

        /** @brief Draws the veil and the panel over the screen. */
        void draw(sf::RenderTarget& target) const;

    private:
        /** @brief Shared application context. Non-owning. */
        AppContext& context;

        /** @brief Keys of the question and of the hint line. */
        hexui::StringKey prompt_key;
        hexui::StringKey hint_key;

        /** @brief The two answers. */
        hexui::Button confirm;
        hexui::Button cancel;

        /** @brief Icon of the affirmative answer. */
        hexgui::Icon confirm_icon;

        /** @brief True while the dialog is open. */
        bool is_open = false;
    };
}

#endif //CONFIRM_DIALOG_H
