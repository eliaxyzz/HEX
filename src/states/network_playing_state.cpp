/**
 * @file network_playing_state.cpp
 * @brief Online match screen implementation.
 */

#include "states/network_playing_state.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ui/sfml_widgets.h"
#include "core/winning_path.h"
#include "ui/animation.h"

namespace hexapp {

    namespace {
        /** @brief Bottom strip reserved for the status line and the commands. */
        constexpr float HUD_HEIGHT = 132.0f;

        constexpr float BUTTON_H = 36.0f;
        constexpr float BUTTON_GAP = 10.0f;
        constexpr float BUTTON_MARGIN = 24.0f;
        constexpr float HUD_MARGIN = 24.0f;

        /** @brief Margin around the board, leaving room for the edge labels. */
        constexpr float BOARD_MARGIN = 56.0f;

        /**
         * @brief Upper bound on waiting for the resignation's outcome, in seconds.
         * @note On a local network the outcome returns within milliseconds. The cap is
         * not for the normal case but for a server that has stopped answering, where
         * the user asked to leave and must be allowed to leave.
         */
        constexpr float LEAVE_GRACE = 0.6f;

        hexgui::HexLayout layoutFor(const int size, const float width, const float height) {
            return hexgui::HexLayout::fit(size, width, height - HUD_HEIGHT, BOARD_MARGIN);
        }
    }

    NetworkPlayingState::NetworkPlayingState(AppContext& context)
        : context(context),
          match(context.match, context.local_name, context.loc),
          renderer(layoutFor(context.match.board_size, context.width, context.height),
                   context.assets.font(), context.theme),
          swap_button(context.loc.text(hexui::StringKey::GAME_PIE_RULE), {}),
          resign_button(context.loc.text(hexui::StringKey::NET_RESIGN), {}),
          leave_button(context.loc.text(hexui::StringKey::NET_LEAVE), {}),
          leave_dialog(context, hexui::StringKey::GAME_LEAVE_PROMPT,
                       hexui::StringKey::GAME_LEAVE_HINT, hexui::StringKey::NET_RESIGN) {
        refreshCommands();
        layoutButtons();
    }

    std::vector<hexui::Button*> NetworkPlayingState::commandButtons() {
        std::vector<hexui::Button*> buttons;
        buttons.reserve(2);
        if (swap_visible) buttons.push_back(&swap_button);
        buttons.push_back(over_visible ? &leave_button : &resign_button);
        return buttons;
    }

    void NetworkPlayingState::layoutButtons() {
        // Fixed widths, measured per label: a button that changes width from one
        // turn to the next makes the whole bar shift.
        constexpr float SWAP_W = 110.0f;
        constexpr float RESIGN_W = 130.0f;
        constexpr float LEAVE_W = 150.0f;

        std::vector<float> widths;
        widths.reserve(2);
        if (swap_visible) widths.push_back(SWAP_W);
        widths.push_back(over_visible ? LEAVE_W : RESIGN_W);

        const std::vector<hexui::Button*> buttons = commandButtons();

        float total = 0.0f;
        for (const float w : widths) total += w + BUTTON_GAP;
        total -= BUTTON_GAP;

        const float y = context.height - BUTTON_H - 20.0f;
        float x = std::max(HUD_MARGIN, context.width - BUTTON_MARGIN - total);

        for (std::size_t i = 0; i < buttons.size(); ++i) {
            buttons[i]->setBounds({x, y, widths[i], BUTTON_H});
            x += widths[i] + BUTTON_GAP;
        }

        leave_dialog.layout();
    }

    void NetworkPlayingState::refreshCommands() {
        // As in the local match: a command that cannot be used is removed rather
        // than disabled. The swap lasts one turn, and a Back to menu button during a
        // running match would be a second way to forfeit that does not say so.
        const bool can_swap = match.canSwap();
        const bool is_over = finished;

        if (can_swap != swap_visible || is_over != over_visible) {
            swap_visible = can_swap;
            over_visible = is_over;
            layoutButtons();
        }

        swap_button.setEnabled(can_swap);
    }

    void NetworkPlayingState::relayout(const float width, const float height) {
        renderer.setLayout(layoutFor(match.boardSize(), width, height));
    }

    void NetworkPlayingState::abandonWith(std::string message) {
        context.net.disconnect();

        // The message travels through the context: the lobby displays it, and the
        // lobby is constructed after this screen has been destroyed.
        context.notice = std::move(message);
        requestTransition(Transition::to(StateId::NETWORK_LOBBY));
    }

    void NetworkPlayingState::leaveMatch() {
        // Nothing to report: the match already ended or the connection already fell.
        if (finished || !context.net.connected()) {
            finishLeaving();
            return;
        }

        // The server accepts a resignation only on the sender's turn; outside that
        // it is refused, and the disconnection is what ends the match on the other
        // side. Either way the reply is awaited and then the screen exits.
        (void)context.net.sendMoveIntent({hexsave::TokenKind::RESIGN, 0, 0});

        leaving = true;
        leaving_elapsed = 0.0f;
    }

    void NetworkPlayingState::finishLeaving() {
        context.net.disconnect();
        requestTransition(Transition::to(StateId::MAIN_MENU));
    }

    void NetworkPlayingState::request(const hexsave::MoveToken& token) {
        if (!context.net.sendMoveIntent(token)) {
            abandonWith(context.loc.text(hexui::StringKey::LOBBY_CONNECTION_LOST) + ": "
                        + context.net.lastError());
        }
    }

    void NetworkPlayingState::adopt(const hexnet::StateUpdate& update) {
        const std::size_t before = match.moveCount();

        if (!match.applyState(update)) {
            // A state that cannot be replayed describes a different match:
            // carrying on with the old position would be worse than stopping.
            abandonWith(context.loc.text(hexui::StringKey::NET_BAD_STATE));
            return;
        }

        // One more stone means a new move, whoever made it: the moment for the
        // sound, exactly as in the local match.
        if (match.moveCount() > before) {
            context.audio.play(hexassets::sounds::PLACE);

            // lastMove tracks the last placement, so after a swap it stays where it
            // was, and an unchanged cell must not reappear out of nowhere.
            if (match.lastMove() && match.lastMove() != appearing) {
                appearing = match.lastMove();
                appear_elapsed = 0.0f;
            }
        }
        seen_moves = match.moveCount();

        if (match.isOver()) {
            finished = true;

            if (winning_path.empty() && match.reason() == hex::EndReason::CONNECTION) {
                winning_path = hexpath::winningPath(match.situation().getBoard(), match.winner());
                win_elapsed = 0.0f;   // il bagliore parte dal primo anello della catena
            }
            if (!win_announced) {
                context.audio.play(hexassets::sounds::WIN);
                win_announced = true;
            }
        }

        refreshCommands();
    }

    void NetworkPlayingState::pumpNetwork() {
        context.net.poll();

        // Drain the inbox first, judge the connection second. The server sends the
        // final state and closes the table immediately after, so outcome and closure
        // arrive in the same pass; checking the socket first discards the result.
        while (const std::optional<hexnet::Incoming> message = context.net.take()) {
            sf::Packet payload = std::move(message->payload);

            switch (message->opcode) {
                case hexnet::Opcode::STATE_UPDATE: {
                    hexnet::StateUpdate update;
                    if (!read(payload, update)) {
                        abandonWith(context.loc.text(hexui::StringKey::NET_UNREADABLE_STATE));
                        return;
                    }
                    adopt(update);
                    if (hasPendingTransition()) return;
                    break;
                }

                case hexnet::Opcode::OPPONENT_LEFT:
                    // Not the player's error: the match ends for a reason outside
                    // their control. The position stays on screen with an explanation
                    // and the return to the menu happens on their command. Being
                    // thrown back to the lobby the instant the other side closes its
                    // window reads as a defect.
                    notice = context.loc.text(hexui::StringKey::LOBBY_OPPONENT_LEFT);
                    finished = true;
                    context.net.disconnect();
                    refreshCommands();
                    return;

                case hexnet::Opcode::ERROR_MSG: {
                    hexnet::ErrorMessage error;

                    // A refusal does not end the match: the server discarded one
                    // request, the position still holds and the move can be retried.
                    notice = read(payload, error) ? error.text
                                                  : context.loc.text(hexui::StringKey::LOBBY_REFUSED);
                    break;
                }

                default:
                    break;   // MATCH_START a partita gia' avviata non aggiunge nulla
            }
        }

        if (context.net.connected()) return;

        // Match over: the server frees the table and closes, which is the expected
        // epilogue rather than a fault, so the final position stays on screen.
        // Likewise someone already leaving has nothing left to lose; update() takes
        // them to the menu.
        if (finished || leaving) return;

        abandonWith(context.net.lastError().empty()
                        ? context.loc.text(hexui::StringKey::LOBBY_CONNECTION_LOST)
                        : context.loc.text(hexui::StringKey::LOBBY_CONNECTION_LOST) + ": "
                              + context.net.lastError());
    }

    void NetworkPlayingState::update(const float dt) {
        appear_elapsed += dt;
        hover_elapsed += dt;
        if (!winning_path.empty()) win_elapsed += dt;

        pumpNetwork();
        if (hasPendingTransition()) return;

        // Exit as soon as the resignation has taken effect, meaning the opponent has
        // its outcome, or as soon as it is clear that it never will.
        if (leaving) {
            leaving_elapsed += dt;

            if (finished || !context.net.connected() || leaving_elapsed >= LEAVE_GRACE)
                finishLeaving();
        }
    }

    void NetworkPlayingState::handleInput(const InputEvent& event) {
        // Already leaving: the match accepts no further command, and a click during
        // the wait must not play a move after the resignation.
        if (leaving && event.type != InputType::RESIZED) return;

        // The confirmation is modal: while open, the match below receives nothing,
        // not even a click on the board.
        switch (leave_dialog.handleInput(event)) {
            case DialogOutcome::CONFIRMED:
                leaveMatch();
                return;
            case DialogOutcome::CONSUMED:
            case DialogOutcome::CANCELLED:
                return;
            case DialogOutcome::IGNORED:
                break;
        }

        switch (event.type) {
            case InputType::RESIZED:
                relayout(event.x, event.y);
                layoutButtons();
                return;

            case InputType::MOUSE_MOVED:
                if (const std::optional<std::pair<int, int>> under =
                        renderer.getLayout().cellAt({event.x, event.y});
                    under != hover) {
                    hover = under;
                    hover_elapsed = 0.0f;
                }
                for (hexui::Button* b : commandButtons()) b->onMouseMove(event.x, event.y);
                return;

            case InputType::MOUSE_LEFT:
                hover.reset();
                for (hexui::Button* b : commandButtons()) b->onMouseLeave();
                return;

            case InputType::MOUSE_PRESSED: {
                if (event.button != MouseButton::LEFT) return;

                // The swap is consulted only while present: when it is not, its
                // rectangle belongs to whichever command slid into its place.
                if (swap_visible && swap_button.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    request(hexnet::NetworkMatch::swapToken());
                    return;
                }
                if (over_visible) {
                    if (leave_button.onMousePress(event.x, event.y)) {
                        context.audio.play(hexassets::sounds::CLICK);

                        // Match over: nothing to forfeit and nobody to notify, so
                        // this simply exits.
                        context.net.disconnect();
                        requestTransition(Transition::to(StateId::MAIN_MENU));
                        return;
                    }
                } else if (resign_button.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    leave_dialog.open();
                    return;
                }

                const auto cell = renderer.getLayout().cellAt({event.x, event.y});
                if (!cell) return;

                // Only ask for what the server would accept: the same rule that
                // draws the preview decides whether the packet is worth sending.
                if (!match.wouldAccept(*cell)) return;

                notice.clear();
                request(hexnet::NetworkMatch::placementToken(*cell));
                return;
            }

            case InputType::KEY_PRESSED:
                // Escape during a running match asks for confirmation: there is a
                // person on the other end, and a stray key must not leave them
                // without an opponent. On a finished match it leaves at once.
                if (event.key == Key::ESCAPE) {
                    if (finished) {
                        context.net.disconnect();
                        requestTransition(Transition::to(StateId::MAIN_MENU));
                    } else {
                        leave_dialog.open();
                    }
                }
                return;

            default:
                return;
        }
    }

    void NetworkPlayingState::draw(sf::RenderTarget& target) {
        hexgui::BoardOverlay overlay;
        overlay.last_move = match.lastMove();
        overlay.winning_path = winning_path;
        overlay.win_elapsed = win_elapsed;

        if (appearing) {
            const float t = hexanim::progress(appear_elapsed, hexanim::PLACEMENT_DURATION);
            overlay.appearing = appearing;
            overlay.appear_progress = hexanim::easeOutCubic(t);
        }

        renderer.draw(target, match.situation().getBoard(), overlay);

        // Preview, shown only where a click would actually be accepted by the server.
        if (hover && match.wouldAccept(*hover)) {
            renderer.drawGhost(target, *hover, pieceOf(match.colour()), hover_elapsed);
        }

        const std::string second = notice.empty()
            ? (finished ? context.loc.text(hexui::StringKey::NET_STILL_CONNECTED)
                        : context.loc.text(hexui::StringKey::NET_OPPONENT) + ": "
                              + match.opponentName())
            : notice;

        const std::vector<std::string> hud{match.statusText(), second};
        renderer.drawHud(target, hud, {HUD_MARGIN, context.height - HUD_HEIGHT + 12.0f});

        // The hint says which of the two things Escape does right now.
        const hexui::Rect hint_area{HUD_MARGIN, context.height - BUTTON_H - 20.0f,
                                    200.0f, BUTTON_H};
        hexgui::drawCenteredText(target,
                                 context.loc.text(over_visible ? hexui::StringKey::GAME_MENU_HINT_OVER
                                                               : hexui::StringKey::NET_LEAVE_HINT),
                                 hint_area, context.assets.font(), 15,
                                 context.theme.hud_secondary);

        for (const hexui::Button* b : commandButtons())
            hexgui::drawButton(target, *b, context.assets, context.theme);

        leave_dialog.draw(target);
    }
}
