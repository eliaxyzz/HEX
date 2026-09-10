/**
 * @file game_server.h
 * @brief Authoritative server: owns the match and enforces its rules.
 *
 * ### What authoritative means here
 *
 * The match exists only on this side. Clients send intents, the server feeds them
 * to the same GameController the local game uses, and returns the resulting state.
 * A modified client cannot obtain more than the engine grants; at worst it gets an
 * ERROR_MSG.
 *
 * ### Never blocks on a socket
 *
 * One sf::SocketSelector watches the listener and both clients together, with a
 * short wait, so the loop can also advance the match through
 * GameController::step() instead of sitting on a read. A client that stops talking
 * cannot freeze the other one.
 *
 * ### One match at a time
 *
 * Two seats, one match: the smallest shape that satisfies the requirement, and
 * keeping it small keeps disconnection handling down to a few cases. When either
 * side drops, the match ends, the survivor is told, and the server goes back to
 * waiting for two fresh players.
 */

#ifndef GAME_SERVER_H
#define GAME_SERVER_H

#include <SFML/Network/SocketSelector.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "core/deferred_player.h"
#include "core/game_controller.h"
#include "network/network_protocol.h"

namespace hexnet {

    /**
     * @brief The player at the far end of a connection.
     *
     * Adds only token-to-move translation on top of the base class: the turn window
     * and the validation are hexplay::DeferredPlayer's, the same ones that govern
     * the human at the window.
     */
    class RemotePlayer final : public hexplay::DeferredPlayer {
    public:
        explicit RemotePlayer(std::string name);

        /**
         * @brief Accepts a move intent arriving from the network.
         * @param token Token received from the client.
         * @return nullopt when accepted, otherwise the rejection reason, ready to be
         * sent back as an ERROR_MSG.
         */
        std::optional<ErrorCode> submitToken(const hexsave::MoveToken& token);
    };

    /** @brief Server side of the network match. */
    class GameServer {
    public:
        /**
         * @brief Builds the server.
         * @param port Port to listen on.
         * @param board_size Board side length of the hosted matches.
         */
        explicit GameServer(unsigned short port = DEFAULT_PORT, int board_size = 11);

        /** @brief Closes the connections and the listener. */
        ~GameServer();

        GameServer(const GameServer&) = delete;
        GameServer& operator=(const GameServer&) = delete;

        /**
         * @brief Starts listening.
         * @return false if the port is unavailable.
         */
        bool listen();

        /**
         * @brief Runs one loop iteration: accept, read, advance the match.
         * @param timeout Upper bound on the wait; never blocks longer.
         */
        void tick(sf::Time timeout = sf::milliseconds(50));

        /** @brief Loops until stop() is called. */
        void run();

        /** @brief Asks the loop to terminate. */
        void stop() { running = false; }

        /** @brief Returns the number of currently connected clients. */
        [[nodiscard]] std::size_t connectedCount() const;

        /** @brief Tests whether a match is in progress. */
        [[nodiscard]] bool matchRunning() const { return controller != nullptr; }

    private:
        /** @brief One seat at the table. */
        struct Slot {
            std::unique_ptr<sf::TcpSocket> socket;
            std::string name;

            /** @brief True after a valid JOIN_REQUEST; before that it is not a player. */
            bool joined = false;

            [[nodiscard]] bool occupied() const { return socket != nullptr; }
        };

        /** @brief Accepts an incoming connection, or refuses it when both seats are taken. */
        void acceptConnection();

        /** @brief Drains the packets ready on the given seat. */
        void readFrom(std::size_t index);

        /** @brief Dispatches an already identified message. */
        void handleMessage(std::size_t index, Opcode opcode, sf::Packet& packet);

        /** @brief Starts the match once both seats hold a joined player. */
        void startMatch();

        /** @brief Advances the match by one step and broadcasts what changed. */
        void advanceMatch();

        /**
         * @brief Opens the turn window of the side to move.
         *
         * @warning Must be called before the state is announced. A player that
         * receives "your turn" and answers immediately has to find the window
         * already open, otherwise its move is refused as out of turn.
         */
        void armCurrentPlayer();

        /**
         * @brief Ends the match and frees both seats.
         * @param reason Text for the log; the clients have already received either
         * the final state or the opponent-left notice.
         */
        void endMatch(const std::string& reason);

        /** @brief Builds the authoritative state, ready to send. */
        [[nodiscard]] StateUpdate currentState() const;

        /** @brief Sends the state to both players. */
        void broadcastState();

        /** @brief Sends a packet to one seat, dropping it if the send fails. */
        void sendTo(std::size_t index, sf::Packet& packet);

        /** @brief Sends an error to one seat. */
        void sendError(std::size_t index, ErrorCode code);

        /** @brief Closes one seat and frees it. */
        void dropClient(std::size_t index, const std::string& reason);

        /** @brief Returns the index of the other seat. */
        [[nodiscard]] static std::size_t other(const std::size_t index) { return 1 - index; }

        /** @brief Listening port. */
        unsigned short port;

        /** @brief Board side length of the hosted matches. */
        int board_size;

        sf::TcpListener listener;
        sf::SocketSelector selector;

        /** @brief The two seats: index 0 plays Red, index 1 plays Blue. */
        std::array<Slot, 2> slots;

        // Declared before the controller, which holds them by reference: members
        // are destroyed in reverse order, so the controller goes first.
        std::unique_ptr<RemotePlayer> red;
        std::unique_ptr<RemotePlayer> blue;

        /** @brief The running match; null while waiting for players. */
        std::unique_ptr<hex::GameController> controller;

        /** @brief Cleared by stop() to break the loop. */
        bool running = true;
    };
}

#endif //GAME_SERVER_H
