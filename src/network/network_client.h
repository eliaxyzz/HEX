/**
 * @file network_client.h
 * @brief Non-blocking client side of the connection.
 *
 * Same discipline as the human player: never blocks. poll() is called once per
 * frame, returns immediately, and queues whatever arrived for the state machine to
 * consume when it sees fit. No method here can cost the interface a frame.
 *
 * The class speaks the protocol and keeps a connection alive, or declares it
 * dropped. It knows nothing about matches: it holds no board, validates no move
 * and decides no turn. Those belong to the server and reach the client already
 * settled inside STATE_UPDATE.
 *
 * @note connect() is the single exception to the non-blocking rule.
 */

#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <cstddef>
#include <deque>
#include <optional>
#include <string>

#include "network/network_protocol.h"

namespace hexnet {

    /** @brief Connection state, as reported to the interface. */
    enum class ConnectionState {
        OFFLINE,     ///< Never connected, or closed on this side.
        CONNECTING,  ///< Attempt in progress.
        CONNECTED,   ///< Connection established.
        FAILED,      ///< Attempt failed, or connection dropped.
    };

    /** @brief A received message, identified but not yet decoded. */
    struct Incoming {
        Opcode opcode = Opcode::ERROR_MSG;

        /**
         * @brief The packet, with the opcode already consumed.
         * @note The consumer calls read() with the message type matching `opcode`.
         */
        sf::Packet payload;
    };

    /**
     * @brief Largest number of messages held in the queue.
     * @note A server, or an impostor, talking without pause must not grow client
     * memory without bound. Past the cap the oldest messages are dropped, since the
     * most recent state is the one that matters.
     */
    inline constexpr std::size_t MAX_QUEUED_MESSAGES = 256;

    /** @brief Connection to the server, advanced one frame at a time. */
    class NetworkClient {
    public:
        NetworkClient();

        /** @brief Closes the connection if one is open. */
        ~NetworkClient();

        NetworkClient(const NetworkClient&) = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;

        /**
         * @brief Attempts to connect to the server.
         *
         * Blocks for up to `timeout`, which must stay short: this is the only call in
         * the class that can wait, and it is issued by a screen that knows it is
         * waiting.
         *
         * @param address Server address or host name.
         * @param port Server port.
         * @param timeout Upper bound on the attempt.
         * @return true if the connection was established.
         */
        bool connect(const std::string& address, unsigned short port,
                     sf::Time timeout = sf::seconds(5));

        /** @brief Closes the connection and clears the queue. */
        void disconnect();

        /** @brief Returns the current connection state. */
        [[nodiscard]] ConnectionState state() const { return status; }

        /** @brief Tests whether the connection is usable right now. */
        [[nodiscard]] bool connected() const { return status == ConnectionState::CONNECTED; }

        /**
         * @brief Returns the reason for the last failure, empty when there is none.
         */
        [[nodiscard]] const std::string& lastError() const { return error; }

        /**
         * @brief Drains every message already waiting on the socket.
         *
         * Returns immediately when there is nothing to read. A disconnection detected
         * here moves the state to FAILED rather than throwing: it is an ordinary
         * outcome the interface must be able to display.
         *
         * @return Number of messages queued by this call.
         */
        std::size_t poll();

        /** @brief Returns the number of messages waiting to be consumed. */
        [[nodiscard]] std::size_t pending() const { return inbox.size(); }

        /**
         * @brief Pops the oldest queued message.
         * @return nullopt if the queue is empty.
         */
        [[nodiscard]] std::optional<Incoming> take();

        /** @brief Introduces the player to the server. */
        bool sendJoinRequest(const std::string& name);

        /** @brief Proposes a move; the answer arrives as a state update or an error. */
        bool sendMoveIntent(const hexsave::MoveToken& token);

    private:
        /**
         * @brief Sends an already composed packet.
         * @note A failed send closes the connection: a half-delivered message would
         * leave client and server describing different games.
         */
        bool send(sf::Packet& packet);

        /** @brief Records a failure and moves the connection to FAILED. */
        void fail(std::string reason);

        /** @brief Socket to the server, non-blocking outside connect(). */
        sf::TcpSocket socket;

        /** @brief Connection state. */
        ConnectionState status = ConnectionState::OFFLINE;

        /** @brief Received messages not yet consumed. */
        std::deque<Incoming> inbox;

        /** @brief Reason for the last failure. */
        std::string error;
    };
}

#endif //NETWORK_CLIENT_H
