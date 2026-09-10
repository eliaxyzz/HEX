/**
 * @file settings_state.h
 * @brief Application preferences screen.
 *
 * Preferences apply immediately: choosing fullscreen shows it happening rather
 * than asking for confirmation. They are also written to file on every change
 * rather than on exit, since a preference meant to survive shutdown must not
 * depend on how the application was shut down.
 */

#ifndef SETTINGS_STATE_H
#define SETTINGS_STATE_H

#include <string>

#include "states/sfml_app_state.h"
#include "ui/ui_widgets.h"

namespace hexapp {

    /** @brief Preferences: display, audio, language, colours and palette. */
    class SettingsState final : public SfmlAppState {
    public:
        /** @brief Builds the screen around the shared context. */
        explicit SettingsState(AppContext& context);

        [[nodiscard]] StateId id() const override { return StateId::SETTINGS; }

        void handleInput(const InputEvent& event) override;
        void draw(sf::RenderTarget& target) override;

    private:
        /**
         * @brief Reapplies the palette locks according to the profile level.
         * @warning Must be called whenever the option group is rebuilt, which a
         * language change does from scratch. Otherwise the locks would vanish on a
         * language switch, unlocking a reward through the least plausible route.
         */
        void refreshPaletteLocks();

        /** @brief Applies and persists the selected palette. */
        void applyPalette();

        /**
         * @brief Writes, under the selector, which level unlocks what is disabled.
         * @note A greyed option without an explanation reads as a fault. The note
         * appears only while something remains locked: at the top level the line
         * removes itself rather than stating something true for nobody.
         */
        void drawPaletteLocks(sf::RenderTarget& target, const sf::Font* font) const;

        /** @brief Shared application context. Non-owning. */
        AppContext& context;

        /** @brief Windowed or fullscreen. */
        hexui::OptionGroup display;

        /**
         * @brief Sound effects on or off.
         * @note This is the effects channel, not a master volume: the music has its
         * own row, and both are labelled so the two are not read as the same switch
         * repeated.
         */
        hexui::OptionGroup audio;

        /** @brief Interface language. */
        hexui::OptionGroup language;

        /**
         * @brief Plain colours, or distinguishing symbols on the stones.
         * @note Takes effect immediately: the shared theme is the very object the
         * game screen hands to the renderer.
         */
        hexui::OptionGroup colours;

        /**
         * @brief Music volume, in steps rather than continuous.
         * @note Four steps instead of a slider: there is no slider widget in this
         * toolkit, and inventing one for a choice that is in practice off, low,
         * normal or high would be more apparatus than control. Steps also take a
         * single click.
         */
        hexui::OptionGroup music;

        /**
         * @brief Board colour scheme: classic, toxic, prestige.
         * @note Locked entries stay visible but disabled: they are rewards, and a
         * hidden reward motivates nobody. The lock is enforced by the widget through
         * setOptionEnabled(), not by the drawing code, so a greyed option cannot be
         * clicked either way.
         */
        hexui::OptionGroup palette;

        /** @brief Returns to the menu. */
        hexui::Button back;

        /** @brief Outcome of the last write, shown only on failure. */
        std::string message;

        /** @brief Recomputes widget placement for the current view size. */
        void layout();

        /**
         * @brief Writes the preferences to file.
         * @note A failure does not revert the choice: the preference holds for this
         * session and the user is told it will not be remembered.
         */
        void persist();

        /** @brief Applies the display mode choice. */
        void applyDisplay();

        /** @brief Applies the audio choice. */
        void applyAudio();

        /**
         * @brief Applies the language choice and rewrites this screen's labels.
         * @note The change shows in the same frame, which is the only way to tell what
         * was chosen, given that the label itself is written in a language.
         */
        void applyLanguage();

        /** @brief Applies and persists the colour accessibility choice. */
        void applyColours();

        /** @brief Applies and persists the music volume. */
        void applyMusic();
    };
}

#endif //SETTINGS_STATE_H
