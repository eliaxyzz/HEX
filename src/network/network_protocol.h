/**
 * @file network_protocol.h
 * @brief Wire protocol spoken by the client and the server.
 *
 * A message is an sf::Packet opening with a one-byte opcode followed by the
 * payload that opcode implies. SFML delivers a packet whole or not at all, so TCP
 * segmentation and byte order are not this file's concern.
 *
 * ### The server is authoritative
 *
 * The client never sends state, only intent (MOVE_INTENT). Legality is decided by
 * the engine on the server, through the same Situation::isValid the local game
 * uses, and the client conforms to the STATE_UPDATE it gets back. A modified
 * client can therefore ask for anything and receive an error rather than an
 * illegal move on the board.
 *
 * ### Every packet is untrusted input
 *
 * The same discipline as the save format, and more so, since the sender is a
 * remote machine. Each read() bounds lengths and counts before allocating,
 * checks that enumerators fall inside their declared range and that coordinates
 * lie within the announced board. A packet failing any check is discarded: the
 * function returns false and the output message keeps its default, never a
 * half-filled one.
 *
 * ### Moves travel as tokens, not as hex::Move
 *
 * hexsave::MoveToken is reused, the same representation as the save file: a
 * placement carries the position only, never the colour. Colour is decided by the
 * turn on the server, so a tampered packet cannot play a stone of the wrong
 * colour, and for the same reason a swap does not carry the cell it swaps.
 */

#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <SFML/Network/Packet.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/game.h"
#include "core/move.h"
#include "core/save_format.h"

namespace hexnet {

    /** @brief Default server port. */
    inline constexpr unsigned short DEFAULT_PORT = 53000;

    /**
     * @brief Protocol version.
     * @note Carried in the join request: two versions cannot understand each other,
     * and saying so upfront beats discovering it mid-match.
     */
    inline constexpr std::uint16_t PROTOCOL_VERSION = 1;

    /** @brief Longest accepted name, in bytes. */
    inline constexpr std::size_t MAX_NAME_LENGTH = hexsave::MAX_NAME_LENGTH;

    /**
     * @brief Largest move count a state update may carry.
     * @note A 26x26 board holds 676 cells and cannot take more than one move each.
     * The bound stops a declared count from driving an arbitrary allocation before
     * a single element has been read.
     */
    inline constexpr std::size_t MAX_MOVES =
        static_cast<std::size_t>(hexsave::MAX_BOARD_SIZE) * hexsave::MAX_BOARD_SIZE;

    /** @brief Message type, carried in the first byte of every packet. */
    enum class Opcode : std::uint8_t {
        JOIN_REQUEST  = 1,  ///< C->S: request to join, with the sender's name.
        MATCH_START   = 2,  ///< S->C: the match begins; here is your colour.
        MOVE_INTENT   = 3,  ///< C->S: request to play this move.
        STATE_UPDATE  = 4,  ///< S->C: the authoritative game state.
        OPPONENT_LEFT = 5,  ///< S->C: the opponent disconnected.
        ERROR_MSG     = 6   ///< S->C: the request was refused.
    };

    /** @brief Reason a request was refused. */
    enum class ErrorCode : std::uint8_t {
        MALFORMED     = 1,  ///< Unreadable packet, or one not expected here.
        VERSION       = 2,  ///< Protocol version mismatch.
        SERVER_FULL   = 3,  ///< Both seats are already taken.
        NOT_YOUR_TURN = 4,  ///< Not the sender's turn to move.
        ILLEGAL_MOVE  = 5   ///< Move rejected by the engine.
    };

    // --- Messages -------------------------------------------------------------

    /** @brief C->S: request to join the match. */
    struct JoinRequest {
        std::uint16_t version = PROTOCOL_VERSION;
        std::string name;

        bool operator==(const JoinRequest&) const = default;
    };

    /** @brief S->C: the match begins. */
    struct MatchStart {
        std::int32_t board_size = 11;

        /** @brief Colour assigned to the recipient. */
        hex::Player colour = hex::Player::RED;

        /** @brief Opponent name, for display. */
        std::string opponent;

        bool operator==(const MatchStart&) const = default;
    };

    /** @brief C->S: intent to play a move. */
    struct MoveIntent {
        hexsave::MoveToken token;

        bool operator==(const MoveIntent&) const = default;
    };

    /**
     * @brief S->C: the authoritative game state.
     *
     * Carries the full history rather than the last move. The cost is small, a few
     * hundred tokens at most, and it makes a client resynchronisable in one step by
     * replaying the moves exactly as loading a save does. A client that misses one
     * update does not stay out of sync forever.
     */
    struct StateUpdate {
        std::int32_t board_size = 11;
        std::vector<hexsave::MoveToken> moves;

        /** @brief Side to move; meaningful only while the match is running. */
        hex::Player to_move = hex::Player::RED;

        /** @brief Whether the match has ended. */
        bool over = false;

        /** @brief Winner and end reason; meaningful only when `over`. */
        hex::Player winner = hex::Player::RED;
        hex::EndReason reason = hex::EndReason::NONE;

        bool operator==(const StateUpdate&) const = default;
    };

    /** @brief S->C: the request was refused. */
    struct ErrorMessage {
        ErrorCode code = ErrorCode::MALFORMED;

        /** @brief Text suitable for display to the user. */
        std::string text;

        bool operator==(const ErrorMessage&) const = default;
    };

    // --- Composition ----------------------------------------------------------

    [[nodiscard]] sf::Packet makeJoinRequest(const JoinRequest& msg);
    [[nodiscard]] sf::Packet makeMatchStart(const MatchStart& msg);
    [[nodiscard]] sf::Packet makeMoveIntent(const MoveIntent& msg);
    [[nodiscard]] sf::Packet makeStateUpdate(const StateUpdate& msg);
    [[nodiscard]] sf::Packet makeOpponentLeft();
    [[nodiscard]] sf::Packet makeError(const ErrorMessage& msg);

    // --- Reading --------------------------------------------------------------

    /**
     * @brief Extracts the opcode heading the packet.
     *
     * Consumes the byte, so a subsequent read() starts at the payload.
     *
     * @return The opcode, or nullopt if the packet is empty or the byte matches no
     * known message.
     */
    [[nodiscard]] std::optional<Opcode> readOpcode(sf::Packet& packet);

    /**
     * @brief Reads the payload, after readOpcode().
     *
     * @return true if the message is complete and passes validation; false if it is
     * truncated, an enumerator is out of range, a length exceeds its bound, or a
     * coordinate falls outside the announced board.
     * @note On false the output message must not be used.
     */
    [[nodiscard]] bool read(sf::Packet& packet, JoinRequest& out);
    [[nodiscard]] bool read(sf::Packet& packet, MatchStart& out);
    [[nodiscard]] bool read(sf::Packet& packet, MoveIntent& out);
    [[nodiscard]] bool read(sf::Packet& packet, StateUpdate& out);
    [[nodiscard]] bool read(sf::Packet& packet, ErrorMessage& out);

    /** @brief Returns a human-readable description of an error code. */
    [[nodiscard]] std::string describe(ErrorCode code);
}

#endif //NETWORK_PROTOCOL_H
