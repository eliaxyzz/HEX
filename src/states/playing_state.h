/**
 * @file playing_state.h
 * @brief Local match screen.
 *
 * Owns everything a match needs: the players, the controller, the observer and the
 * renderer. Leaving the screen destroys all of them, and any search still running
 * is cancelled and joined by the AbstractPlayer destructor.
 */

#ifndef PLAYING_STATE_H
#define PLAYING_STATE_H

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/game_clock.h"
#include "core/game_controller.h"
#include "states/confirm_dialog.h"
#include "core/hex_geometry.h"
#include "core/player.h"
#include "states/sfml_app_state.h"
#include "ui/sfml_game_observer.h"
#include "ui/sfml_human_player.h"
#include "ui/sfml_renderer.h"
#include "ui/ui_widgets.h"

namespace hexapp {

    /** @brief Running match, configured by the menu choices. */
    class PlayingState final : public SfmlAppState {
    public:
        /**
         * @brief Prepares a match from the settings held in the context.
         * @param context Window, fonts, theme and menu choices.
         */
        explicit PlayingState(AppContext& context);

        [[nodiscard]] StateId id() const override { return StateId::PLAYING; }

        void handleInput(const InputEvent& event) override;
        void update(float dt) override;
        void draw(sf::RenderTarget& target) override;

    private:
        /** @brief Shared application context. Non-owning. */
        AppContext& context;

        // Declared before the controller, which holds them by reference: members are
        // destroyed in reverse order, so the controller goes first.

        /** @brief Red's player; always present. */
        std::unique_ptr<hex::AbstractPlayer> red;

        /** @brief Blue's player; human or engine, depending on the mode. */
        std::unique_ptr<hex::AbstractPlayer> blue;

        /**
         * @brief The same players, when human, for routing clicks.
         * @note Non-owning; null when that colour is played by the engine.
         */
        hexgui::SfmlHumanPlayer* human_red = nullptr;
        hexgui::SfmlHumanPlayer* human_blue = nullptr;

        /** @brief Match engine. */
        hex::GameController controller;

        /** @brief Visual state, driven by the controller's events. */
        hexgui::SfmlGameObserver observer;

        /**
         * @brief Match clock, one budget per colour.
         * @note Runs only for the side to move. On expiry the controller declares the
         * loss: this screen watches the clock, it does not referee.
         */
        hexplay::GameClock clock;

        /** @brief Board renderer. */
        hexgui::SfmlBoardRenderer renderer;

        /** @brief Cell under the pointer, when the pointer is over the board. */
        std::optional<std::pair<int, int>> hover;

        /** @brief Result of the last step, driving the thinking indicator. */
        hex::StepResult last_step = hex::StepResult::WAITING;

        /**
         * @brief Match commands, drawn in the bottom strip.
         *
         * These are the primary interface. Keyboard shortcuts remain as
         * accelerators, but a command that exists only as a key does not exist for
         * anyone who has not read about it somewhere.
         *
         * @note There is deliberately no undo command. The engine keeps the move
         * history because it needs it to replay a save and to restore a position, but
         * that history is not exposed to the player: the engine does not revisit its
         * mistakes and neither does the human.
         */
        hexui::Button swap_button;
        hexui::Button new_game_button;
        hexui::Button leave_button;
        hexui::Button save_button;

        /**
         * @brief Whether the swap still applies, and hence whether its button exists.
         * @note Outside the turn where it is legal the command is not drawn and takes
         * no room in the bar: it disappears rather than greying out. A disabled button
         * is a promise the interface does not keep, and the pie rule lasts one turn,
         * so it would sit grey for the rest of the match.
         */
        bool swap_visible = false;

        /**
         * @brief True once the match is over, which the command bar reflects.
         * @note A running match can be left; a finished one can be replayed. Two
         * different actions at two different moments, neither meaningful at the
         * other's, so the bar shows only the applicable one instead of keeping both
         * with one disabled.
         */
        bool over_visible = false;

        /** @brief Returns the commands present right now, in drawing order. */
        [[nodiscard]] std::vector<hexui::Button*> commandButtons();

        /**
         * @brief Realigns the command bar with the match state.
         * @note Called every frame, since the swap and the restart depend on
         * conditions that change from turn to turn. The bar is re-laid out whenever
         * its composition changes.
         */
        void refreshCommands();

        /** @brief Outcome of the last save, shown in the status bar. */
        std::string save_message;

        // --- Named saving ---
        //
        // A match is saved to saves/[name].hex, with the name collected by a modal
        // prompt. There is no single slot: silently overwriting the only saved game
        // would be data loss nobody asked for.

        /** @brief True while the name prompt is open. */
        bool save_prompt = false;

        /** @brief Name typed for the save. */
        hexui::TextInput save_name;

        /** @brief Confirm and cancel actions of the prompt. */
        hexui::Button save_confirm;
        hexui::Button save_cancel;

        /** @brief Opens the name prompt with an empty field. */
        void openSavePrompt();

        /** @brief Closes the prompt without saving. */
        void closeSavePrompt();

        /**
         * @brief Writes the match under the typed name, if it is usable.
         * @note A name that is empty or made entirely of rejected characters leaves
         * the prompt open with an explanation: closing it would lose the match and
         * the name together.
         */
        void confirmSave();

        /** @brief Routes an event into the prompt; true if it was consumed. */
        bool handleSavePromptInput(const InputEvent& event);

        /** @brief Draws the prompt over the match. */
        void drawSavePrompt(sf::RenderTarget& target);

        /** @brief Writes the current match to saves/[slot].hex. */
        void saveGame(const std::string& slot);

        /** @brief Advances the clock and ends the match if it expires. */
        void tickClock(float dt);

        /**
         * @brief Records the outcome in the user's tally, exactly once.
         * @note Against the engine only: between two humans on one machine a win has
         * no owner, and counting both sides of the same match would make the tally
         * meaningless.
         */
        void recordOutcome();

        /** @brief True once this match's outcome has been tallied. */
        bool outcome_recorded = false;

        /**
         * @brief Starts the rematch with the colours swapped.
         *
         * Does not unwind the history: it derives the rematch settings through
         * rematchOf() and goes through StateId::PLAYING again, so the new match is
         * born on the same path as one started from the menu. Players, controller,
         * clocks and observer are new because they belong to a different match.
         */
        void startRematch();

        /**
         * @brief Asks for confirmation before leaving the match.
         * @note A running match is work done, and a stray Escape must not discard it
         * unasked. The same dialog class as the online match, not a lookalike copy.
         */
        ConfirmDialog leave_dialog;

        /**
         * @brief Returns the cell to highlight.
         * @note The observer only knows the moves it saw go by; after a load the match
         * already exists, so this falls back on the last move in the history.
         */
        [[nodiscard]] std::optional<std::pair<int, int>> highlightCell() const;

        /** @brief Repositions the commands on screen. */
        void layoutButtons();

        // --- Animation and end of match ---

        /** @brief Cell currently appearing, when an animation is running. */
        std::optional<std::pair<int, int>> appearing;

        /** @brief Seconds elapsed since the animation started. */
        float appear_elapsed = 0.0f;

        /**
         * @brief Seconds the pointer has rested on the same cell.
         * @note Reset on every move, so the preview's pulse restarts at full on the
         * new cell instead of inheriting the phase of the one just left.
         */
        float hover_elapsed = 0.0f;

        /**
         * @brief Seconds since the winning chain appeared.
         * @note Drives the glow travelling along it; stays at zero while there is none.
         */
        float win_elapsed = 0.0f;

        /** @brief Moves played as of the previous frame, to detect new ones. */
        std::size_t seen_moves = 0;

        /**
         * @brief Winning chain, computed once when the match ends.
         * @note Empty while playing and after an undo.
         */
        std::vector<std::pair<int, int>> winning_path;

        /** @brief Starts or cancels animations according to how the history changed. */
        void syncAnimation();

        /** @brief Computes or forgets the winning chain per the match state. */
        void syncWinningPath();

        /** @brief Returns the human player currently being asked, if any. */
        [[nodiscard]] hexgui::SfmlHumanPlayer* armedHuman();

        /** @brief Tests whether the given colour is played by a human. */
        [[nodiscard]] bool isHuman(hex::Player p) const;

        /** @brief Recomputes the board layout for the given view size. */
        void relayout(float width, float height);
    };
}

#endif //PLAYING_STATE_H
