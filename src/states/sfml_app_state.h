/**
 * @file sfml_app_state.h
 * @brief Bridge between the state machine and SFML.
 *
 * AppState knows nothing of SFML. Here it gains rendering, plus the context that
 * outlives screen changes.
 */

#ifndef SFML_APP_STATE_H
#define SFML_APP_STATE_H

#include <SFML/Graphics.hpp>

#include <utility>
#include <vector>

#include "core/app_preferences.h"
#include "core/player_profile.h"
#include "states/app_state.h"
#include "ui/asset_manager.h"
#include "ui/background_renderer.h"
#include "ui/audio_player.h"
#include "core/game_settings.h"
#include "core/localization.h"
#include "network/network_client.h"
#include "network/network_protocol.h"
#include "core/move.h"
#include "ui/sfml_theme.h"

namespace hexapp {

    /**
     * @brief State shared by the screens and outliving them.
     * @note Passed by reference; the context is owned by the application.
     */
    struct AppContext {
        /** @brief Window everything is drawn on. */
        sf::RenderWindow& window;

        /** @brief Shared fonts, textures and sounds, loaded once. */
        hexassets::AssetManager& assets;

        /**
         * @brief Sound effect player.
         * @note Shared like the assets, so muting it on one screen mutes it
         * everywhere, which is what a mute switch is expected to do.
         */
        hexgui::AudioPlayer& audio;

        /**
         * @brief Connection to the server during online play.
         * @note Lives in the context because it must survive the transition: the
         * lobby opens it and the match uses it. Stays OFFLINE outside network play.
         */
        hexnet::NetworkClient& net;

        /**
         * @brief Interface strings in the selected language.
         * @note A single shared instance, so changing the language in the settings
         * changes it everywhere.
         */
        hexui::LocalizationManager& loc;

        /** @brief Active colour palette. */
        hexgui::Theme theme{};

        /**
         * @brief Animated backdrop of the boardless screens.
         * @note Owned by the context rather than by each screen because its clock
         * must run across transitions: a per-screen instance would restart the
         * pattern on every change, and it would show.
         */
        hexgui::BackgroundRenderer backdrop{};

        /** @brief Current view width, in pixels. */
        float width = 0.0f;

        /** @brief Current view height, in pixels. */
        float height = 0.0f;

        /**
         * @brief Choices made in the menu.
         * @note In the context because they must survive the transition: the menu
         * writes them, the match reads them.
         */
        GameSettings settings{};

        /**
         * @brief Moves to replay when the next match starts.
         * @note Empty for a new match, filled by the menu when a save is loaded. The
         * game screen consumes it and clears it.
         */
        std::vector<hex::Move> pending_moves;

        /**
         * @brief Cells to wall off when the next match starts.
         *
         * Travels beside pending_moves for the same reason: a loaded match must
         * resume on its own board, not a fresh one. Empty for a normal match and for
         * a newly created Arcade one, where the game screen draws the black holes
         * itself; populated only when loading an Arcade save, which carries its
         * holes with it.
         */
        std::vector<std::pair<int, int>> pending_holes;

        /**
         * @brief Application preferences, read at startup.
         * @note The settings screen edits them and writes them back; every other
         * screen only reads them.
         */
        Preferences prefs{};

        /**
         * @brief Player progression: experience and level.
         *
         * Sits beside the preferences because it shares their lifetime, read at
         * startup and rewritten on change, but lives in a separate file: preferences
         * are reversible choices, this is what the player earned. The match updates
         * it, the menu displays it.
         */
        Profile profile{};

        /**
         * @brief Assignment received from the server for the online match.
         * @note Written by the lobby when MATCH_START arrives, read by the game
         * screen as soon as it is constructed.
         */
        hexnet::MatchStart match{};

        /** @brief Name this client introduced itself to the server with. */
        std::string local_name;

        /**
         * @brief Message to display on the next screen.
         *
         * A match that breaks off must be able to explain why, but it dies before
         * there is a screen to write on. It leaves the message here and whoever
         * arrives next picks it up and clears it.
         */
        std::string notice;
    };

    /**
     * @brief A screen that can also draw itself.
     * @note The type the graphical build's StateMachine is instantiated on.
     */
    class SfmlAppState : public AppState {
    public:
        /** @brief Draws the screen. Called once per frame. */
        virtual void draw(sf::RenderTarget& target) = 0;
    };
}

#endif //SFML_APP_STATE_H
