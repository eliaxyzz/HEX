/**
 * @file game_server.cpp
 * @brief Authoritative server implementation.
 */

#include "server/game_server.h"

#include <iostream>
#include <utility>

#include "core/save_format.h"

namespace hexnet {

    namespace {
        /** @brief Writes a log line under a recognisable prefix. */
        void log(const std::string& text) {
            std::cout << "[server] " << text << '\n';
        }

        /** @brief Returns a fallback name for a client that sends none. */
        std::string nameOr(const std::string& proposed, const char* fallback) {
            return proposed.empty() ? fallback : proposed;
        }
    }

    // --- RemotePlayer ---------------------------------------------------------

    RemotePlayer::RemotePlayer(std::string name) : DeferredPlayer(std::move(name)) {}

    std::optional<ErrorCode> RemotePlayer::submitToken(const hexsave::MoveToken& token) {
        // Outside the turn window there is not even a position to interpret the
        // token in, so answer before attempting it.
        if (!isArmed() || hasQueuedMove()) return ErrorCode::NOT_YOUR_TURN;

        std::string why;
        const std::optional<hex::Move> move = hexsave::moveFromToken(askedSituation(), token, why);
        if (!move) return ErrorCode::ILLEGAL_MOVE;

        // The engine has the final word, exactly as it does for a click.
        return submit(*move) ? std::nullopt : std::optional{ErrorCode::ILLEGAL_MOVE};
    }

    // --- GameServer -----------------------------------------------------------

    GameServer::GameServer(const unsigned short port, const int board_size)
        : port(port), board_size(board_size) {}

    GameServer::~GameServer() {
        for (std::size_t i = 0; i < slots.size(); ++i) {
            if (slots[i].occupied()) dropClient(i, "arresto del server");
        }
        listener.close();
    }

    bool GameServer::listen() {
        if (listener.listen(port) != sf::Socket::Status::Done) {
            std::cerr << "[server] porta " << port << " non disponibile\n";
            return false;
        }

        listener.setBlocking(false);
        selector.add(listener);

        log("in ascolto sulla porta " + std::to_string(port));
        return true;
    }

    std::size_t GameServer::connectedCount() const {
        std::size_t count = 0;
        for (const Slot& slot : slots) {
            if (slot.occupied()) ++count;
        }
        return count;
    }

    void GameServer::tick(const sf::Time timeout) {
        // The wait is short by design: without one the loop would spin on a core,
        // and with a long one the match would not advance until somebody spoke.
        if (selector.wait(timeout)) {
            if (selector.isReady(listener)) acceptConnection();

            for (std::size_t i = 0; i < slots.size(); ++i) {
                if (slots[i].occupied() && selector.isReady(*slots[i].socket)) readFrom(i);
            }
        }

        if (controller) advanceMatch();
    }

    void GameServer::run() {
        while (running) tick();
    }

    void GameServer::acceptConnection() {
        auto socket = std::make_unique<sf::TcpSocket>();
        if (listener.accept(*socket) != sf::Socket::Status::Done) return;

        // Find a free seat, if any.
        std::size_t free_slot = slots.size();
        for (std::size_t i = 0; i < slots.size(); ++i) {
            if (!slots[i].occupied()) {
                free_slot = i;
                break;
            }
        }

        if (free_slot == slots.size()) {
            // Say no explicitly rather than letting a dying connection imply it.
            // This send is blocking because the socket is closed right afterwards.
            sf::Packet packet = makeError({ErrorCode::SERVER_FULL, describe(ErrorCode::SERVER_FULL)});
            (void)socket->send(packet);
            socket->disconnect();
            log("connessione rifiutata: partita al completo");
            return;
        }

        socket->setBlocking(false);
        selector.add(*socket);

        slots[free_slot].socket = std::move(socket);
        slots[free_slot].joined = false;
        slots[free_slot].name.clear();

        log("connessione accettata sul posto " + std::to_string(free_slot));
    }

    void GameServer::readFrom(const std::size_t index) {
        Slot& slot = slots[index];

        while (true) {
            sf::Packet packet;
            const sf::Socket::Status result = slot.socket->receive(packet);

            if (result == sf::Socket::Status::NotReady) return;

            if (result == sf::Socket::Status::Disconnected) {
                dropClient(index, "disconnessione");
                return;
            }
            if (result != sf::Socket::Status::Done) {
                dropClient(index, "errore di ricezione");
                return;
            }

            const std::optional<Opcode> opcode = readOpcode(packet);
            if (!opcode) {
                sendError(index, ErrorCode::MALFORMED);
                continue;
            }

            handleMessage(index, *opcode, packet);

            // handleMessage may have closed the seat, leaving no socket behind.
            if (!slots[index].occupied()) return;
        }
    }

    void GameServer::handleMessage(const std::size_t index, const Opcode opcode, sf::Packet& packet) {
        switch (opcode) {
            case Opcode::JOIN_REQUEST: {
                JoinRequest join;
                if (!read(packet, join)) {
                    sendError(index, ErrorCode::MALFORMED);
                    return;
                }
                if (join.version != PROTOCOL_VERSION) {
                    sendError(index, ErrorCode::VERSION);
                    dropClient(index, "versione del protocollo incompatibile");
                    return;
                }
                if (slots[index].joined) return;   // joining twice changes nothing

                slots[index].name = nameOr(join.name,
                                           index == 0 ? "Giocatore 1" : "Giocatore 2");
                slots[index].joined = true;
                log("giocatore \"" + slots[index].name + "\" al posto " + std::to_string(index));

                if (!controller) startMatch();
                return;
            }

            case Opcode::MOVE_INTENT: {
                MoveIntent intent;
                if (!read(packet, intent)) {
                    sendError(index, ErrorCode::MALFORMED);
                    return;
                }
                if (!controller) {
                    sendError(index, ErrorCode::NOT_YOUR_TURN);
                    return;
                }

                RemotePlayer& player = (index == 0) ? *red : *blue;
                if (const std::optional<ErrorCode> refused = player.submitToken(intent.token)) {
                    sendError(index, *refused);
                }
                // Once accepted, the move enters play on the next step() and both
                // clients see it arrive as state rather than as an acknowledgement.
                return;
            }

            default:
                // The remaining opcodes travel server to client; receiving one here
                // means the peer is not a client of this game.
                sendError(index, ErrorCode::MALFORMED);
                return;
        }
    }

    void GameServer::startMatch() {
        if (!slots[0].joined || !slots[1].joined) return;

        red = std::make_unique<RemotePlayer>(slots[0].name);
        blue = std::make_unique<RemotePlayer>(slots[1].name);
        controller = std::make_unique<hex::GameController>(board_size, *red, *blue);

        for (std::size_t i = 0; i < slots.size(); ++i) {
            MatchStart start;
            start.board_size = board_size;
            start.colour = (i == 0) ? hex::Player::RED : hex::Player::BLUE;
            start.opponent = slots[other(i)].name;

            sf::Packet packet = makeMatchStart(start);
            sendTo(i, packet);
        }

        log("partita avviata: " + slots[0].name + " (Rosso) contro " + slots[1].name + " (Blu)");

        // Arm the first player before announcing the state. See armCurrentPlayer().
        armCurrentPlayer();
        broadcastState();
    }

    void GameServer::armCurrentPlayer() {
        // A DeferredPlayer accepts a move only between startMove and tryTakeMove,
        // and step() is what opens that window. Announcing "your turn" beforehand
        // invites a reply that would be refused as out of turn, costing the move to
        // whoever answers fastest.
        //
        // The step below asks the side to move for a move and returns WAITING at
        // once, since nobody has decided anything yet. From then on an incoming
        // intent is acceptable.
        if (controller && !controller->isOver()) (void)controller->step();
    }

    void GameServer::advanceMatch() {
        const hex::StepResult result = controller->step();

        // Only a real step changes the state; broadcasting every iteration would
        // fill the link with identical copies.
        if (result == hex::StepResult::PLAYED) {
            // Open the new side-to-move's window first, then tell it the turn is
            // its own. The reverse order is a race lost by the fastest responder.
            armCurrentPlayer();
            broadcastState();
        }

        if (controller->isOver()) {
            const std::optional<hex::GameResult> outcome = controller->result();
            const std::string winner = outcome && outcome->winner == hex::Player::RED
                                           ? slots[0].name : slots[1].name;
            endMatch("vittoria di " + winner);
        }
    }

    StateUpdate GameServer::currentState() const {
        StateUpdate state;
        state.board_size = controller->situation().getBoard().getSize();
        state.moves = hexsave::tokensFromMoves(controller->moveHistory());
        state.to_move = controller->situation().toMove();
        state.over = controller->isOver();

        if (const std::optional<hex::GameResult> outcome = controller->result()) {
            state.winner = outcome->winner;
            state.reason = outcome->reason;
        }

        return state;
    }

    void GameServer::broadcastState() {
        const StateUpdate state = currentState();

        for (std::size_t i = 0; i < slots.size(); ++i) {
            if (!slots[i].occupied()) continue;

            sf::Packet packet = makeStateUpdate(state);
            sendTo(i, packet);
        }
    }

    void GameServer::sendTo(const std::size_t index, sf::Packet& packet) {
        Slot& slot = slots[index];
        if (!slot.occupied()) return;

        while (true) {
            const sf::Socket::Status result = slot.socket->send(packet);

            if (result == sf::Socket::Status::Done) return;
            if (result == sf::Socket::Status::Partial) continue;

            dropClient(index, "invio non riuscito");
            return;
        }
    }

    void GameServer::sendError(const std::size_t index, const ErrorCode code) {
        sf::Packet packet = makeError({code, describe(code)});
        sendTo(index, packet);
    }

    void GameServer::dropClient(const std::size_t index, const std::string& reason) {
        Slot& slot = slots[index];
        if (!slot.occupied()) return;

        const std::string who = slot.name.empty() ? ("posto " + std::to_string(index)) : slot.name;

        selector.remove(*slot.socket);
        slot.socket->disconnect();
        slot.socket.reset();
        slot.joined = false;
        slot.name.clear();

        log(who + ": " + reason);

        // A match with one player is meaningless: tell the survivor now rather
        // than leaving it waiting for a turn that will never come.
        if (controller) {
            const std::size_t survivor = other(index);
            if (slots[survivor].occupied()) {
                sf::Packet packet = makeOpponentLeft();
                sendTo(survivor, packet);
            }
            endMatch("abbandono");
        }
    }

    void GameServer::endMatch(const std::string& reason) {
        if (!controller) return;

        log("partita conclusa: " + reason);

        // The controller is released before the players it holds by reference;
        // their destructor aborts any request still open.
        controller.reset();
        red.reset();
        blue.reset();

        // Clear the table: whoever is still connected is dismissed, so the next
        // match starts from two fresh connections and inherits no partial state.
        for (std::size_t i = 0; i < slots.size(); ++i) {
            if (!slots[i].occupied()) continue;

            selector.remove(*slots[i].socket);
            slots[i].socket->disconnect();
            slots[i].socket.reset();
            slots[i].joined = false;
            slots[i].name.clear();
        }

        log("in attesa di due giocatori");
    }
}
