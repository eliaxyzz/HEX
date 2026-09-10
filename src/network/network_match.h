/**
 * @file network_match.h
 * @brief The online match as seen by the client.
 *
 * ### Why there is no GameController here
 *
 * The client does not referee; the server does, and there is exactly one of it.
 * What is needed on this side is the current position, and that arrives inside
 * STATE_UPDATE as the complete move history.
 *
 * Every update rebuilds the position from scratch, replaying the moves exactly as
 * loading a save does. That is what makes the client resynchronisable: a lost
 * packet does not leave a wrong board for the rest of the match, because the next
 * update repairs it. A local GameController, which can only be advanced forward
 * one move at a time, would remove that guarantee.
 *
 * ### What stays local
 *
 * The rebuilt Situation answers everything the interface needs: whose turn it is,
 * whether a cell is playable, whether the swap is available, who won. Same
 * questions as the local game under the same rules, so a move preview cannot offer
 * something the server would refuse.
 *
 * @note The outcome comes from the server rather than from the reconstruction: a
 * win by timeout or by forfeit leaves no trace in the move history, so replaying
 * it would not surface it.
 * @note SFML-free: pure logic, and covered by the test suite as such.
 */

#ifndef NETWORK_MATCH_H
#define NETWORK_MATCH_H

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/game.h"
#include "core/localization.h"
#include "core/move.h"
#include "network/network_protocol.h"

namespace hexnet {

    /** @brief Online match state from this client's point of view. */
    class NetworkMatch {
    public:
        /**
         * @brief Builds the match from the assignment received from the server.
         * @param start The MATCH_START message: board size, colour, opponent.
         * @param local_name Name chosen by this player.
         * @param loc Interface strings, used for the status line.
         */
        NetworkMatch(const MatchStart& start, std::string local_name,
                     const hexui::LocalizationManager& loc);

        /**
         * @brief Adopts the authoritative state that just arrived.
         *
         * Rejects an update announcing a different board size, or a history that
         * cannot be replayed: either way that packet does not describe this match,
         * and adopting it halfway would leave the interface showing an invented
         * position.
         *
         * @param update State received from the server.
         * @return true if the state was adopted.
         */
        bool applyState(const StateUpdate& update);

        // --- Queries for the interface ---

        /** @brief Returns the current position. */
        [[nodiscard]] const hex::Situation& situation() const { return current; }

        /** @brief Returns the board side length. */
        [[nodiscard]] int boardSize() const { return board_size; }

        /** @brief Returns the colour the server assigned to this client. */
        [[nodiscard]] hex::Player colour() const { return my_colour; }

        /** @brief Return the local and the opponent name. */
        [[nodiscard]] const std::string& localName() const { return local; }
        [[nodiscard]] const std::string& opponentName() const { return opponent; }

        /** @brief Returns the number of moves played so far. */
        [[nodiscard]] std::size_t moveCount() const { return played; }

        /** @brief Tests whether it is this player's turn. */
        [[nodiscard]] bool isMyTurn() const;

        /** @brief Tests whether this player can invoke the pie rule right now. */
        [[nodiscard]] bool canSwap() const;

        /**
         * @brief Tests whether a click on that cell would produce a legal move.
         * @note Applies the same rule the server will, so a preview never promises
         * something that would be refused.
         */
        [[nodiscard]] bool wouldAccept(std::pair<int, int> pos) const;

        /** @brief Returns the token to send for a placement on that cell. */
        [[nodiscard]] static hexsave::MoveToken placementToken(std::pair<int, int> pos);

        /** @brief Returns the pie swap token. */
        [[nodiscard]] static hexsave::MoveToken swapToken();

        /** @brief Returns the last occupied cell, for highlighting. */
        [[nodiscard]] const std::optional<std::pair<int, int>>& lastMove() const { return last; }

        /** @brief Tests whether the server considers the match over. */
        [[nodiscard]] bool isOver() const { return over; }

        /** @brief Return the winner and end reason; meaningful only once over. */
        [[nodiscard]] hex::Player winner() const { return victor; }
        [[nodiscard]] hex::EndReason reason() const { return end_reason; }

        /** @brief Tests whether this player won. */
        [[nodiscard]] bool wonByLocal() const { return over && victor == my_colour; }

        /**
         * @brief Builds the status line: whose turn it is, or the outcome.
         * @note Lives here rather than in the screen because it is a function of the
         * state, and is therefore verifiable without opening a window.
         */
        [[nodiscard]] std::string statusText() const;

    private:
        /** @brief Returns the name of the player holding the given colour. */
        [[nodiscard]] const std::string& nameOf(hex::Player p) const;

        /** @brief Interface strings, for the status line. Non-owning. */
        const hexui::LocalizationManager& loc;

        int board_size;
        hex::Player my_colour;
        std::string local;
        std::string opponent;

        /** @brief Position rebuilt from the last authoritative state. */
        hex::Situation current;

        /** @brief Number of moves in the adopted history. */
        std::size_t played = 0;

        /** @brief Last occupied cell, for highlighting. */
        std::optional<std::pair<int, int>> last;

        // The outcome comes from the server: the reconstruction knows nothing of
        // matches closed by the referee, through timeout or disconnection.
        bool over = false;
        hex::Player victor = hex::Player::RED;
        hex::EndReason end_reason = hex::EndReason::NONE;
    };
}

#endif //NETWORK_MATCH_H
