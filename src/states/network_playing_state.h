/**
 * @file network_playing_state.h
 * @brief Online match: draws the state the server sends.
 *
 * The difference from PlayingState is not cosmetic. There is no GameController
 * here, no players to advance, and nothing to save or restart: there is a position
 * arriving over the network and a click that becomes a request. Keeping the two
 * screens apart is what stops the local one filling up with "if we are online"
 * branches.
 *
 * @note Rendering is identical either way: SfmlBoardRenderer takes a board and
 * draws it, and does not care where it came from.
 */

#ifndef NETWORK_PLAYING_STATE_H
#define NETWORK_PLAYING_STATE_H

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "network/network_match.h"
#include "states/confirm_dialog.h"
#include "states/sfml_app_state.h"
#include "ui/sfml_renderer.h"
#include "ui/ui_widgets.h"

namespace hexapp {

    /** @brief A match played against a remote opponent. */
    class NetworkPlayingState final : public SfmlAppState {
    public:
        /**
         * @brief Builds the screen around the match just assigned.
         * @param context Shared context; context.match carries the lobby's result:
         * board size, colour and opponent name.
         */
        explicit NetworkPlayingState(AppContext& context);

        [[nodiscard]] StateId id() const override { return StateId::NETWORK_PLAYING; }

        void handleInput(const InputEvent& event) override;
        void update(float dt) override;
        void draw(sf::RenderTarget& target) override;

    private:
        AppContext& context;

        /** @brief Match state, rebuilt on every update. */
        hexnet::NetworkMatch match;

        /** @brief Board renderer. */
        hexgui::SfmlBoardRenderer renderer;

        /** @brief Cell under the pointer, when the pointer is over the board. */
        std::optional<std::pair<int, int>> hover;

        /**
         * @brief Online match commands, under the same rules as the local ones.
         *
         * The bar shows what can be done now rather than the game's command
         * inventory: the swap exists only on the turn where it is legal, and leaving
         * and returning to the menu exclude each other. Mid-match, leaving is a
         * forfeit, and naming it otherwise would make it look free.
         */
        hexui::Button swap_button;
        hexui::Button resign_button;
        hexui::Button leave_button;

        /** @brief True while the swap is playable, hence while its button exists. */
        bool swap_visible = false;

        /** @brief True once the match has ended: leaving replaces forfeiting. */
        bool over_visible = false;

        /**
         * @brief True once there is nothing left to play, however the match ended.
         *
         * Not the same as match.isOver(): an opponent walking away ends the match
         * without any outcome arriving from the server. Either way what remains is
         * the final position, and the only command that still makes sense is
         * returning to the menu.
         *
         * @note This is also what separates a connection that drops from one closed
         * because it is no longer needed: once the match is over the server frees the
         * table and disconnects both sides. Treating that closure as an error would
         * send a player who had just won back to the lobby with "connection lost".
         */
        bool finished = false;

        /**
         * @brief The same confirmation as the local match, before leaving.
         * @note It matters more here: there is a person on the other end who would be
         * left without an opponent.
         */
        ConfirmDialog leave_dialog;

        /** @brief Returns the commands present right now, in drawing order. */
        [[nodiscard]] std::vector<hexui::Button*> commandButtons();

        /**
         * @brief Begins leaving the match: sends the resignation and awaits its effect.
         *
         * Resignation is the exit notice the protocol knows, and it gives the
         * opponent a real win instead of a vanished opponent.
         *
         * @warning The resignation must be allowed to land. Closing the socket right
         * after sending makes the server notice the disconnection first and discard
         * the packet, so the opponent receives OPPONENT_LEFT and the resignation
         * never existed. The screen therefore stays a few more frames, long enough
         * for the final state to come back, and only then closes.
         * @note The wait blocks nothing: update() carries it forward one frame at a
         * time and it is capped regardless. Leaving cannot depend on a server
         * answering.
         */
        void leaveMatch();

        /** @brief Closes the connection and returns to the menu immediately. */
        void finishLeaving();

        /** @brief True while waiting for the resignation to take effect. */
        bool leaving = false;

        /** @brief Seconds since leaving was requested. */
        float leaving_elapsed = 0.0f;

        /** @brief Transient message: a server refusal or a warning. */
        std::string notice;

        /** @brief Moves seen as of the previous frame, to detect new ones. */
        std::size_t seen_moves = 0;

        /**
         * @brief Cell currently appearing, when an animation is running.
         * @note The opponent's stone arrives over the network rather than from a
         * click, but must appear the same way. The animation matters most here: it is
         * the only thing signalling a move the user did not make.
         */
        std::optional<std::pair<int, int>> appearing;

        /** @brief Seconds elapsed since the appearance animation started. */
        float appear_elapsed = 0.0f;

        /** @brief Seconds the pointer has rested on the same cell. */
        float hover_elapsed = 0.0f;

        /** @brief Seconds since the winning chain appeared. */
        float win_elapsed = 0.0f;

        /** @brief Winning chain, computed once when the match ends. */
        std::vector<std::pair<int, int>> winning_path;

        /** @brief True once the victory sound has played; it plays only once. */
        bool win_announced = false;

        /** @brief Drains the messages arrived from the server. */
        void pumpNetwork();

        /** @brief Adopts an authoritative state and reacts to what changed. */
        void adopt(const hexnet::StateUpdate& update);

        /** @brief Ends the match and returns to the lobby carrying a message. */
        void abandonWith(std::string message);

        /** @brief Sends a move request, handling a dropped connection. */
        void request(const hexsave::MoveToken& token);

        /** @brief Realigns command availability with the match state. */
        void refreshCommands();

        /** @brief Repositions the commands on screen. */
        void layoutButtons();

        /** @brief Recomputes the board layout. */
        void relayout(float width, float height);
    };
}

#endif //NETWORK_PLAYING_STATE_H
