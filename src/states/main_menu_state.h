/**
 * @file main_menu_state.h
 * @brief Opening screen, where a match is configured.
 */

#ifndef MAIN_MENU_STATE_H
#define MAIN_MENU_STATE_H

#include <string>

#include "states/sfml_app_state.h"
#include "ui/ui_widgets.h"

namespace hexapp {

    /** @brief Menu: mode, player names, difficulty and launch. */
    class MainMenuState final : public SfmlAppState {
    public:
        /** @brief Builds the menu around the shared context. */
        explicit MainMenuState(AppContext& context);

        [[nodiscard]] StateId id() const override { return StateId::MAIN_MENU; }

        void onEnter() override;
        void handleInput(const InputEvent& event) override;
        void update(float dt) override;
        void draw(sf::RenderTarget& target) override;

    private:
        /** @brief Shared application context. Non-owning. */
        AppContext& context;

        /** @brief Human versus engine, or human versus human. */
        hexui::OptionGroup mode;

        /** @brief First player's name; plays Red. */
        hexui::TextInput nickname;

        /**
         * @brief Second player's name, shown only in human versus human.
         * @note Occupies the slot of the difficulty selector, which has no meaning in
         * that mode.
         */
        hexui::TextInput nickname2;

        /** @brief Engine level; shown only in human versus engine. */
        hexui::OptionGroup difficulty;

        /**
         * @brief Colour played by the human; shown only against the engine.
         * @note Meaningless between two humans, where the first name is Red. The
         * third entry, Random, is not a colour but the choice not to pick one.
         */
        hexui::OptionGroup side;

        /**
         * @brief Rule variant: normal match or Arcade.
         * @note Sits beside the mode rather than in the settings because it is a
         * choice about this match, like the level or the colour: it changes from one
         * match to the next, not once and for all.
         */
        hexui::OptionGroup variant;

        /** @brief Starts the match. */
        hexui::Button play;

        /** @brief Opens the save slot list. */
        hexui::Button load;

        /** @brief Opens the lobby for network play. */
        hexui::Button online;

        /** @brief Opens the application preferences. */
        hexui::Button settings;

        /** @brief Opens the guided onboarding. */
        hexui::Button tutorial;

        /** @brief Closes the application. */
        hexui::Button quit;

        /** @brief Message shown after a load attempt. */
        std::string message;

        /** @brief Seconds since the screen was entered, driving the intro fade. */
        float entered_since = 0.0f;

        /** @brief Tests whether the difficulty choice is relevant. */
        [[nodiscard]] bool aiEnabled() const { return mode.selected() == 0; }

        /** @brief Returns the focused text field, if any. */
        [[nodiscard]] hexui::TextInput* focusedField();

        /** @brief Recomputes widget placement for the current view size. */
        void layout();

        /**
         * @brief Rewrites the labels in the active language.
         * @note Widgets own their text, so switching language is not enough to change
         * what they display. Done on screen entry, which is when the user returns
         * from the settings.
         */
        void applyLanguage();

        /**
         * @brief Resolves the human's colour from the current selection.
         * @note Random tosses the coin here, at match start: drawing it earlier would
         * already be a choice, merely a hidden one.
         */
        [[nodiscard]] hex::Player chosenColour() const;

        /** @brief Copies the choices into the context and starts the match. */
        void startGame();

        /** @brief Return the intro fade opacity and its residual vertical offset. */
        [[nodiscard]] float introOpacity() const;
        [[nodiscard]] float introOffset() const;
    };
}

#endif //MAIN_MENU_STATE_H
