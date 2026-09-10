/**
 * @file network_lobby_state.h
 * @brief Connection screen for an online match.
 *
 * Does three things: collects the address and the name, attempts the connection,
 * and waits for the server to announce the start. Everything else, validating
 * moves and owning the match, belongs elsewhere.
 *
 * @note The connection attempt is the only blocking call in the whole application
 * and lasts a few seconds at most. From then on waiting is non-blocking, one
 * poll() per frame, so the screen stays alive and redrawable while the opponent
 * has yet to arrive.
 */

#ifndef NETWORK_LOBBY_STATE_H
#define NETWORK_LOBBY_STATE_H

#include <string>

#include "states/sfml_app_state.h"
#include "ui/ui_widgets.h"

namespace hexapp {

    /** @brief Lobby: address, name, connection, waiting for the opponent. */
    class NetworkLobbyState final : public SfmlAppState {
    public:
        explicit NetworkLobbyState(AppContext& context);

        [[nodiscard]] StateId id() const override { return StateId::NETWORK_LOBBY; }

        void handleInput(const InputEvent& event) override;
        void update(float dt) override;
        void draw(sf::RenderTarget& target) override;

    private:
        /** @brief Phase the screen is in. */
        enum class Phase {
            FORM,       ///< Filling in the fields.
            CONNECTING, ///< Attempt under way; lasts one frame, since connect blocks.
            WAITING     ///< Connected, waiting for the server to form the match.
        };

        AppContext& context;

        /** @brief Server address; empty means the local machine. */
        hexui::TextInput address;

        /** @brief Name to introduce ourselves to the server with. */
        hexui::TextInput nickname;

        /** @brief Starts the connection attempt. */
        hexui::Button connect;

        /** @brief Returns to the menu, closing any open connection. */
        hexui::Button back;

        Phase phase = Phase::FORM;

        /** @brief Status line shown below the fields. */
        std::string status;

        /** @brief True when the status line describes a problem. */
        bool status_is_error = false;

        /** @brief Returns the focused field, if any. */
        [[nodiscard]] hexui::TextInput* focusedField();

        /** @brief Recomputes widget placement. */
        void layout();

        /** @brief Attempts the connection and the join handshake. */
        void attemptConnection();

        /** @brief Drains incoming messages; may start the match or fail. */
        void pumpNetwork();

        /** @brief Records a problem and returns to the form, closing the connection. */
        void failWith(std::string message);

        /** @brief Returns the address to use: the one typed, or the local machine. */
        [[nodiscard]] std::string effectiveAddress() const;

        /** @brief Returns the name to use: the one typed, or a fallback. */
        [[nodiscard]] std::string effectiveName() const;
    };
}

#endif //NETWORK_LOBBY_STATE_H
