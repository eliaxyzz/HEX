/**
 * @file network_client.cpp
 * @brief Network client implementation.
 */

#include "network/network_client.h"

#include <utility>

namespace hexnet {

    NetworkClient::NetworkClient() = default;

    NetworkClient::~NetworkClient() {
        disconnect();
    }

    bool NetworkClient::connect(const std::string& address, const unsigned short port,
                                const sf::Time timeout) {
        disconnect();

        const std::optional<sf::IpAddress> resolved = sf::IpAddress::resolve(address);
        if (!resolved) {
            fail("indirizzo non valido: " + address);
            return false;
        }

        status = ConnectionState::CONNECTING;

        // The attempt must be blocking, otherwise it returns NotReady at once and
        // there is no way to tell whether the server is there. The socket stays
        // non-blocking from here on.
        socket.setBlocking(true);
        const sf::Socket::Status result = socket.connect(*resolved, port, timeout);
        socket.setBlocking(false);

        if (result != sf::Socket::Status::Done) {
            fail("nessuna risposta da " + address + ":" + std::to_string(port));
            return false;
        }

        status = ConnectionState::CONNECTED;
        error.clear();
        return true;
    }

    void NetworkClient::disconnect() {
        socket.disconnect();
        inbox.clear();
        status = ConnectionState::OFFLINE;
    }

    void NetworkClient::fail(std::string reason) {
        socket.disconnect();
        status = ConnectionState::FAILED;
        error = std::move(reason);
    }

    std::size_t NetworkClient::poll() {
        if (status != ConnectionState::CONNECTED) return 0;

        std::size_t received = 0;

        // Read while anything is ready: NotReady ends the loop without waiting and
        // is the normal case in a frame where nothing arrives.
        while (true) {
            sf::Packet packet;
            const sf::Socket::Status result = socket.receive(packet);

            if (result == sf::Socket::Status::NotReady) break;

            if (result == sf::Socket::Status::Disconnected) {
                fail("il server ha chiuso la connessione");
                break;
            }
            if (result != sf::Socket::Status::Done) {
                fail("errore di ricezione");
                break;
            }

            const std::optional<Opcode> opcode = readOpcode(packet);
            if (!opcode) continue;   // unintelligible packet: drop it and carry on

            // The queue is capped, and it is the oldest message that is dropped:
            // the state that matters is always the most recent one.
            if (inbox.size() >= MAX_QUEUED_MESSAGES) inbox.pop_front();

            inbox.push_back(Incoming{*opcode, std::move(packet)});
            ++received;
        }

        return received;
    }

    std::optional<Incoming> NetworkClient::take() {
        if (inbox.empty()) return std::nullopt;

        Incoming front = std::move(inbox.front());
        inbox.pop_front();
        return front;
    }

    bool NetworkClient::send(sf::Packet& packet) {
        if (status != ConnectionState::CONNECTED) return false;

        // On a non-blocking socket a send can complete partially; SFML remembers
        // the offset and expects the call to be repeated with the same packet.
        while (true) {
            const sf::Socket::Status result = socket.send(packet);

            if (result == sf::Socket::Status::Done) return true;
            if (result == sf::Socket::Status::Partial) continue;

            fail(result == sf::Socket::Status::Disconnected
                     ? "il server ha chiuso la connessione"
                     : "invio non riuscito");
            return false;
        }
    }

    bool NetworkClient::sendJoinRequest(const std::string& name) {
        JoinRequest msg;
        msg.name = name.size() > MAX_NAME_LENGTH ? name.substr(0, MAX_NAME_LENGTH) : name;

        sf::Packet packet = makeJoinRequest(msg);
        return send(packet);
    }

    bool NetworkClient::sendMoveIntent(const hexsave::MoveToken& token) {
        sf::Packet packet = makeMoveIntent(MoveIntent{token});
        return send(packet);
    }
}
