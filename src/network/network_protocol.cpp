/**
 * @file network_protocol.cpp
 * @brief Message composition and bounds-checked reading.
 */

#include "network/network_protocol.h"

#include <utility>

namespace hexnet {

    namespace {
        /** @brief Highest valid enumerator of each wire enum, for range checks. */
        constexpr std::uint8_t LAST_OPCODE = static_cast<std::uint8_t>(Opcode::ERROR_MSG);
        constexpr std::uint8_t LAST_ERROR_CODE = static_cast<std::uint8_t>(ErrorCode::ILLEGAL_MOVE);
        constexpr std::uint8_t LAST_TOKEN_KIND = static_cast<std::uint8_t>(hexsave::TokenKind::RESIGN);
        constexpr std::uint8_t LAST_END_REASON = static_cast<std::uint8_t>(hex::EndReason::ILLEGAL_MOVE);
        constexpr std::uint8_t LAST_PLAYER = static_cast<std::uint8_t>(hex::Player::BLUE);

        /** @brief Opens a packet with the given opcode. */
        sf::Packet opened(const Opcode code) {
            sf::Packet packet;
            packet << static_cast<std::uint8_t>(code);
            return packet;
        }

        /** @brief Writes a move token: kind followed by coordinates. */
        void writeToken(sf::Packet& packet, const hexsave::MoveToken& token) {
            packet << static_cast<std::uint8_t>(token.kind)
                   << static_cast<std::int32_t>(token.row)
                   << static_cast<std::int32_t>(token.col);
        }

        /**
         * @brief Reads a token and checks it against the announced board.
         *
         * A placement must fall inside the board. A swap or a resignation has no
         * coordinates, so theirs are zeroed rather than believed: equivalent tokens
         * then compare equal, and a sender cannot smuggle information through fields
         * it has no use for.
         */
        bool readToken(sf::Packet& packet, const std::int32_t board_size, hexsave::MoveToken& out) {
            std::uint8_t kind = 0;
            std::int32_t row = 0;
            std::int32_t col = 0;

            if (!(packet >> kind >> row >> col)) return false;
            if (kind > LAST_TOKEN_KIND) return false;

            out.kind = static_cast<hexsave::TokenKind>(kind);

            if (out.kind != hexsave::TokenKind::ADD) {
                out.row = 0;
                out.col = 0;
                return true;
            }

            if (row < 0 || row >= board_size || col < 0 || col >= board_size) return false;

            out.row = static_cast<int>(row);
            out.col = static_cast<int>(col);
            return true;
        }

        /** @brief Reads a player, rejecting values outside the enum. */
        bool readPlayer(sf::Packet& packet, hex::Player& out) {
            std::uint8_t value = 0;
            if (!(packet >> value) || value > LAST_PLAYER) return false;

            out = static_cast<hex::Player>(value);
            return true;
        }

        /** @brief Reads a board size the engine would accept. */
        bool readBoardSize(sf::Packet& packet, std::int32_t& out) {
            std::int32_t size = 0;
            if (!(packet >> size)) return false;
            if (size < hexsave::MIN_BOARD_SIZE || size > hexsave::MAX_BOARD_SIZE) return false;

            out = size;
            return true;
        }

        /** @brief Reads a name, enforcing the length bound. */
        bool readName(sf::Packet& packet, std::string& out) {
            std::string name;
            if (!(packet >> name) || name.size() > MAX_NAME_LENGTH) return false;

            out = std::move(name);
            return true;
        }
    }

    // --- Composition ----------------------------------------------------------

    sf::Packet makeJoinRequest(const JoinRequest& msg) {
        sf::Packet packet = opened(Opcode::JOIN_REQUEST);
        packet << msg.version << msg.name;
        return packet;
    }

    sf::Packet makeMatchStart(const MatchStart& msg) {
        sf::Packet packet = opened(Opcode::MATCH_START);
        packet << msg.board_size
               << static_cast<std::uint8_t>(msg.colour)
               << msg.opponent;
        return packet;
    }

    sf::Packet makeMoveIntent(const MoveIntent& msg) {
        sf::Packet packet = opened(Opcode::MOVE_INTENT);
        writeToken(packet, msg.token);
        return packet;
    }

    sf::Packet makeStateUpdate(const StateUpdate& msg) {
        sf::Packet packet = opened(Opcode::STATE_UPDATE);
        packet << msg.board_size
               << static_cast<std::uint32_t>(msg.moves.size());

        for (const hexsave::MoveToken& token : msg.moves) writeToken(packet, token);

        packet << static_cast<std::uint8_t>(msg.to_move)
               << msg.over
               << static_cast<std::uint8_t>(msg.winner)
               << static_cast<std::uint8_t>(msg.reason);
        return packet;
    }

    sf::Packet makeOpponentLeft() {
        return opened(Opcode::OPPONENT_LEFT);
    }

    sf::Packet makeError(const ErrorMessage& msg) {
        sf::Packet packet = opened(Opcode::ERROR_MSG);
        packet << static_cast<std::uint8_t>(msg.code) << msg.text;
        return packet;
    }

    // --- Reading --------------------------------------------------------------

    std::optional<Opcode> readOpcode(sf::Packet& packet) {
        std::uint8_t code = 0;
        if (!(packet >> code)) return std::nullopt;
        if (code == 0 || code > LAST_OPCODE) return std::nullopt;

        return static_cast<Opcode>(code);
    }

    bool read(sf::Packet& packet, JoinRequest& out) {
        JoinRequest msg;
        if (!(packet >> msg.version)) return false;
        if (!readName(packet, msg.name)) return false;

        out = std::move(msg);
        return true;
    }

    bool read(sf::Packet& packet, MatchStart& out) {
        MatchStart msg;
        if (!readBoardSize(packet, msg.board_size)) return false;
        if (!readPlayer(packet, msg.colour)) return false;
        if (!readName(packet, msg.opponent)) return false;

        out = std::move(msg);
        return true;
    }

    bool read(sf::Packet& packet, MoveIntent& out) {
        // An intent does not carry the board size: the server owns the match and
        // knows it. The bound here only discards absurd coordinates; real validity
        // is settled by the engine.
        MoveIntent msg;
        if (!readToken(packet, hexsave::MAX_BOARD_SIZE, msg.token)) return false;

        out = msg;
        return true;
    }

    bool read(sf::Packet& packet, StateUpdate& out) {
        StateUpdate msg;
        if (!readBoardSize(packet, msg.board_size)) return false;

        std::uint32_t count = 0;
        if (!(packet >> count)) return false;

        // Check the bound before reserving: an inflated count is the cheapest way
        // to make a peer allocate memory on demand.
        if (count > MAX_MOVES) return false;

        msg.moves.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            hexsave::MoveToken token;
            if (!readToken(packet, msg.board_size, token)) return false;
            msg.moves.push_back(token);
        }

        if (!readPlayer(packet, msg.to_move)) return false;
        if (!(packet >> msg.over)) return false;
        if (!readPlayer(packet, msg.winner)) return false;

        std::uint8_t reason = 0;
        if (!(packet >> reason) || reason > LAST_END_REASON) return false;
        msg.reason = static_cast<hex::EndReason>(reason);

        out = std::move(msg);
        return true;
    }

    bool read(sf::Packet& packet, ErrorMessage& out) {
        std::uint8_t code = 0;
        if (!(packet >> code)) return false;
        if (code == 0 || code > LAST_ERROR_CODE) return false;

        ErrorMessage msg;
        msg.code = static_cast<ErrorCode>(code);
        if (!readName(packet, msg.text)) return false;

        out = std::move(msg);
        return true;
    }

    std::string describe(const ErrorCode code) {
        switch (code) {
            case ErrorCode::VERSION:       return "versione del protocollo diversa";
            case ErrorCode::SERVER_FULL:   return "la partita e' gia' al completo";
            case ErrorCode::NOT_YOUR_TURN: return "non e' il tuo turno";
            case ErrorCode::ILLEGAL_MOVE:  return "mossa non valida";
            default:                       return "messaggio non compreso";
        }
    }
}
