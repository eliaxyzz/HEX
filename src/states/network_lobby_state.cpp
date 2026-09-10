/**
 * @file network_lobby_state.cpp
 * @brief Network lobby implementation.
 */

#include "states/network_lobby_state.h"

#include <utility>

#include "network/network_protocol.h"
#include "ui/sfml_widgets.h"

namespace hexapp {

    namespace {
        /** @brief Width of the widget column. */
        constexpr float COLUMN_W = 420.0f;

        /** @brief Height of one widget row. */
        constexpr float ROW_H = 46.0f;

        /** @brief Vertical gap between consecutive rows. */
        constexpr float GAP = 18.0f;

        /** @brief Address used when the field is left empty. */
        constexpr const char* LOCAL_HOST = "127.0.0.1";

        /** @brief Longest hand-typed address accepted. */
        constexpr std::size_t ADDRESS_MAX = 40;
    }

    NetworkLobbyState::NetworkLobbyState(AppContext& context)
        : context(context),
          address(context.loc.text(hexui::StringKey::LOBBY_ADDRESS), {}, ADDRESS_MAX),
          nickname(context.loc.text(hexui::StringKey::LOBBY_NAME), {}, 16),
          connect(context.loc.text(hexui::StringKey::LOBBY_CONNECT), {}),
          back(context.loc.text(hexui::StringKey::BACK), {}) {

        // A message left behind by an interrupted match is shown here: this is where
        // the user lands, and the only moment they can read it.
        if (!context.notice.empty()) {
            status = context.notice;
            status_is_error = true;
            context.notice.clear();
        }

        layout();
    }

    void NetworkLobbyState::layout() {
        const float x = (context.width - COLUMN_W) / 2.0f;
        float y = context.height * 0.34f;

        const auto row = [&](const float height) {
            const hexui::Rect r{x, y, COLUMN_W, height};
            y += height + GAP;
            return r;
        };

        address.setBounds(row(ROW_H));
        nickname.setBounds(row(ROW_H));

        y += GAP;
        connect.setBounds(row(ROW_H + 8.0f));

        const hexui::Rect wide = row(ROW_H);
        back.setBounds({wide.x + COLUMN_W / 4.0f, wide.y, COLUMN_W / 2.0f, wide.h});
    }

    hexui::TextInput* NetworkLobbyState::focusedField() {
        if (address.focused()) return &address;
        if (nickname.focused()) return &nickname;
        return nullptr;
    }

    std::string NetworkLobbyState::effectiveAddress() const {
        return address.text().empty() ? LOCAL_HOST : address.text();
    }

    std::string NetworkLobbyState::effectiveName() const {
        return nickname.text().empty() ? context.loc.text(hexui::StringKey::NAME_GUEST)
                                       : nickname.text();
    }

    void NetworkLobbyState::failWith(std::string message) {
        context.net.disconnect();
        status = std::move(message);
        status_is_error = true;
        phase = Phase::FORM;
    }

    void NetworkLobbyState::attemptConnection() {
        phase = Phase::CONNECTING;
        status = context.loc.text(hexui::StringKey::LOBBY_CONNECTING);
        status_is_error = false;

        // The attempt blocks for a few seconds at most, and is issued from a screen
        // that has just announced it is waiting.
        if (!context.net.connect(effectiveAddress(), hexnet::DEFAULT_PORT)) {
            failWith(context.loc.text(hexui::StringKey::LOBBY_CONNECT_FAILED) + ": "
                     + context.net.lastError());
            return;
        }

        if (!context.net.sendJoinRequest(effectiveName())) {
            failWith(context.loc.text(hexui::StringKey::LOBBY_SERVER_CLOSED));
            return;
        }

        phase = Phase::WAITING;
        status = context.loc.text(hexui::StringKey::LOBBY_WAITING);
    }

    void NetworkLobbyState::pumpNetwork() {
        context.net.poll();

        // Report a connection lost while waiting at once: otherwise it is
        // indistinguishable from an opponent taking their time.
        if (!context.net.connected()) {
            failWith(context.net.lastError().empty()
                         ? context.loc.text(hexui::StringKey::LOBBY_CONNECTION_LOST)
                         : context.loc.text(hexui::StringKey::LOBBY_CONNECTION_LOST) + ": "
                               + context.net.lastError());
            return;
        }

        while (const std::optional<hexnet::Incoming> message = context.net.take()) {
            sf::Packet payload = std::move(message->payload);

            switch (message->opcode) {
                case hexnet::Opcode::MATCH_START: {
                    hexnet::MatchStart start;
                    if (!read(payload, start)) {
                        failWith(context.loc.text(hexui::StringKey::LOBBY_BAD_START));
                        return;
                    }

                    // The match details travel through the context: the screen that
                    // uses them is constructed after this transition.
                    context.match = start;
                    context.local_name = effectiveName();
                    requestTransition(Transition::to(StateId::NETWORK_PLAYING));
                    return;
                }

                case hexnet::Opcode::ERROR_MSG: {
                    hexnet::ErrorMessage error;
                    const std::string text = read(payload, error) ? error.text : std::string{};
                    failWith(context.loc.text(hexui::StringKey::LOBBY_REFUSED)
                             + (text.empty() ? "" : ": " + text));
                    return;
                }

                case hexnet::Opcode::OPPONENT_LEFT:
                    failWith(context.loc.text(hexui::StringKey::LOBBY_OPPONENT_LEFT));
                    return;

                default:
                    // A state arriving before the start is of no use: the match
                    // proper begins with MATCH_START.
                    break;
            }
        }
    }

    void NetworkLobbyState::update(const float dt) {
        address.update(dt);
        nickname.update(dt);

        if (phase == Phase::WAITING) pumpNetwork();
    }

    void NetworkLobbyState::handleInput(const InputEvent& event) {
        switch (event.type) {
            case InputType::RESIZED:
                layout();
                return;

            case InputType::MOUSE_MOVED:
                address.onMouseMove(event.x, event.y);
                nickname.onMouseMove(event.x, event.y);
                connect.onMouseMove(event.x, event.y);
                back.onMouseMove(event.x, event.y);
                return;

            case InputType::MOUSE_LEFT:
                address.onMouseLeave();
                nickname.onMouseLeave();
                connect.onMouseLeave();
                back.onMouseLeave();
                return;

            case InputType::MOUSE_PRESSED: {
                if (event.button != MouseButton::LEFT) return;

                address.onMousePress(event.x, event.y);
                nickname.onMousePress(event.x, event.y);

                if (connect.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    attemptConnection();
                } else if (back.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    context.net.disconnect();
                    requestTransition(Transition::to(StateId::MAIN_MENU));
                }
                return;
            }

            case InputType::TEXT_ENTERED:
                if (hexui::TextInput* field = focusedField()) field->onCharacter(event.text);
                return;

            case InputType::KEY_PRESSED:
                if (event.key == Key::BACKSPACE) {
                    if (hexui::TextInput* field = focusedField()) field->onBackspace();
                    return;
                }
                if (event.key == Key::ENTER && phase == Phase::FORM) {
                    attemptConnection();
                    return;
                }
                if (event.key == Key::ESCAPE) {
                    // With a field focused, Escape leaves the field; otherwise it
                    // leaves the lobby and closes the connection.
                    if (hexui::TextInput* field = focusedField()) {
                        field->setFocused(false);
                        return;
                    }
                    context.net.disconnect();
                    requestTransition(Transition::to(StateId::MAIN_MENU));
                }
                return;

            default:
                return;
        }
    }

    void NetworkLobbyState::draw(sf::RenderTarget& target) {
        context.backdrop.draw(target, context.width, context.height);

        const hexgui::Theme& t = context.theme;
        const sf::Font* font = context.assets.font();

        const auto line = [&](const std::string& text, const float y, const unsigned size,
                              const sf::Color colour) {
            hexgui::drawCenteredText(target, text, {0.0f, y, context.width, size * 1.4f},
                                     font, size, colour);
        };

        hexgui::drawGlowText(target, context.loc.text(hexui::StringKey::LOBBY_TITLE),
                             {0.0f, context.height * 0.15f, context.width, 52 * 1.4f},
                             font, 52, t.hud_primary, t.ui_accent, 6.0f);
        line(context.loc.text(hexui::StringKey::LOBBY_HINT),
             context.height * 0.245f, 15, t.hud_secondary);

        hexgui::drawFlatTextInput(target, address, font, t, hexgui::Icon::ONLINE);
        hexgui::drawFlatTextInput(target, nickname, font, t, hexgui::Icon::USER);

        // While waiting there is nothing to reconnect: the button is disabled rather
        // than offering an action that would open a second connection.
        connect.setEnabled(phase == Phase::FORM);

        hexgui::drawFlatButton(target, connect, font, t, hexgui::ButtonStyle::PRIMARY,
                               hexgui::Icon::PLAY);
        hexgui::drawFlatButton(target, back, font, t, hexgui::ButtonStyle::GHOST,
                               hexgui::Icon::EXIT);

        if (!status.empty()) {
            line(status, back.bounds().y + back.bounds().h + GAP, 17,
                 status_is_error ? t.ui_accent : t.hud_secondary);
        }
    }
}
