/**
 * @file test_network.cpp
 * @brief Wire protocol: composition, reading, and rejection of broken packets.
 *
 * There is no socket in here. Composing a packet and reading it back is a pure
 * function and is verified as one: the network-dependent part, accepting
 * connections and waiting for data, adds nothing to these rules and would take two
 * processes to exercise.
 *
 * Half the checks concern malformed packets. The sender is a remote machine, so
 * every field is potentially hostile and rejection is the behaviour that matters
 * most.
 */

#include "network/network_protocol.h"

#include <cstdint>
#include <string>
#include <vector>

#include "core/deferred_player.h"
#include "core/game.h"
#include "core/localization.h"
#include "network/network_match.h"
#include "core/save_format.h"
#include "test_framework.h"

using namespace hexnet;
using hexsave::MoveToken;
using hexsave::TokenKind;

namespace {

    /** @brief Default-language strings: the match composes its own status line. */
    const hexui::LocalizationManager loc;

    /** @brief Builds a placement token. */
    MoveToken add(const int row, const int col) {
        return {TokenKind::ADD, row, col};
    }

    /** @brief Reads the opcode then the message, as a real receiver does. */
    template <typename Message>
    bool roundTrip(sf::Packet packet, const Opcode expected, Message& out) {
        const std::optional<Opcode> opcode = readOpcode(packet);
        if (!opcode || *opcode != expected) return false;
        return read(packet, out);
    }
}

void network() {

    // --- Opcode: the first byte, and no others exist ---
    {
        sf::Packet packet = makeOpponentLeft();
        const std::optional<Opcode> opcode = readOpcode(packet);
        CHECK(opcode == Opcode::OPPONENT_LEFT, "opcode: riconosciuto");
        CHECK(packet.endOfPacket(), "opcode: OPPONENT_LEFT non ha carico utile");
    }
    {
        sf::Packet empty;
        CHECK(readOpcode(empty) == std::nullopt, "opcode: pacchetto vuoto rifiutato");
    }
    {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(0);
        CHECK(readOpcode(packet) == std::nullopt, "opcode: lo zero non e' un messaggio");
    }
    {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(200);
        CHECK(readOpcode(packet) == std::nullopt, "opcode: valore fuori enum rifiutato");
    }

    // --- JOIN_REQUEST ---
    {
        const JoinRequest sent{PROTOCOL_VERSION, "Elia"};
        JoinRequest got;
        CHECK(roundTrip(makeJoinRequest(sent), Opcode::JOIN_REQUEST, got), "join: rileggibile");
        CHECK(got == sent, "join: identico all'originale");
    }
    {
        // The version travels as-is: the receiver decides what to do with it, and the
        // server uses it to dismiss a client of another version.
        const JoinRequest sent{99, "Vecchio"};
        JoinRequest got;
        CHECK(roundTrip(makeJoinRequest(sent), Opcode::JOIN_REQUEST, got), "join: versione diversa leggibile");
        CHECK(got.version == 99, "join: la versione arriva intatta");
    }
    {
        // An oversized name is an attempt to force an allocation, not a name.
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::JOIN_REQUEST)
               << PROTOCOL_VERSION
               << std::string(MAX_NAME_LENGTH + 1, 'x');

        (void)readOpcode(packet);
        JoinRequest got;
        CHECK(!read(packet, got), "join: nome oltre il tetto rifiutato");
    }
    {
        // Truncated packet: the opcode is there, the rest is missing.
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::JOIN_REQUEST);

        (void)readOpcode(packet);
        JoinRequest got;
        CHECK(!read(packet, got), "join: pacchetto troncato rifiutato");
    }

    // --- MATCH_START ---
    {
        const MatchStart sent{11, hex::Player::BLUE, "Avversario"};
        MatchStart got;
        CHECK(roundTrip(makeMatchStart(sent), Opcode::MATCH_START, got), "start: rileggibile");
        CHECK(got == sent, "start: identico all'originale");
        CHECK(got.colour == hex::Player::BLUE, "start: il colore assegnato arriva");
    }
    {
        // A board size the engine would reject must not even get in.
        for (const std::int32_t bad : {0, 1, 27, -5}) {
            sf::Packet packet;
            packet << static_cast<std::uint8_t>(Opcode::MATCH_START)
                   << bad << static_cast<std::uint8_t>(0) << std::string("x");

            (void)readOpcode(packet);
            MatchStart got;
            CHECK(!read(packet, got), "start: lato " + std::to_string(bad) + " rifiutato");
        }
    }
    {
        // A non-existent colour would make every later comparison undefined.
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::MATCH_START)
               << static_cast<std::int32_t>(11)
               << static_cast<std::uint8_t>(7)
               << std::string("x");

        (void)readOpcode(packet);
        MatchStart got;
        CHECK(!read(packet, got), "start: colore fuori enum rifiutato");
    }

    // --- MOVE_INTENT ---
    {
        MoveIntent got;
        CHECK(roundTrip(makeMoveIntent(MoveIntent{add(3, 7)}), Opcode::MOVE_INTENT, got),
              "intento: rileggibile");
        CHECK(got.token == add(3, 7), "intento: coordinate intatte");
    }
    {
        MoveIntent got;
        CHECK(roundTrip(makeMoveIntent(MoveIntent{{TokenKind::PIE, 0, 0}}),
                        Opcode::MOVE_INTENT, got), "intento: lo scambio viaggia");
        CHECK(got.token.kind == TokenKind::PIE, "intento: scambio riconosciuto");
    }
    {
        MoveIntent got;
        CHECK(roundTrip(makeMoveIntent(MoveIntent{{TokenKind::RESIGN, 0, 0}}),
                        Opcode::MOVE_INTENT, got), "intento: la resa viaggia");
        CHECK(got.token.kind == TokenKind::RESIGN, "intento: resa riconosciuta");
    }
    {
        // Swap and resignation carry no coordinates: whatever arrives is zeroed, so
        // nobody can hide information in fields that serve no purpose.
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::MOVE_INTENT)
               << static_cast<std::uint8_t>(TokenKind::PIE)
               << static_cast<std::int32_t>(9) << static_cast<std::int32_t>(9);

        (void)readOpcode(packet);
        MoveIntent got;
        CHECK(read(packet, got), "intento: scambio con coordinate accettato");
        CHECK(got.token.row == 0 && got.token.col == 0, "intento: coordinate dello scambio azzerate");
    }
    {
        // Absurd coordinates: the bound here is the largest possible board, and real
        // validity is settled by the engine, which holds the match.
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::MOVE_INTENT)
               << static_cast<std::uint8_t>(TokenKind::ADD)
               << static_cast<std::int32_t>(-1) << static_cast<std::int32_t>(0);

        (void)readOpcode(packet);
        MoveIntent got;
        CHECK(!read(packet, got), "intento: riga negativa rifiutata");
    }
    {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::MOVE_INTENT)
               << static_cast<std::uint8_t>(99)
               << static_cast<std::int32_t>(0) << static_cast<std::int32_t>(0);

        (void)readOpcode(packet);
        MoveIntent got;
        CHECK(!read(packet, got), "intento: tipo di mossa fuori enum rifiutato");
    }

    // --- STATE_UPDATE ---
    {
        StateUpdate sent;
        sent.board_size = 11;
        sent.moves = {add(0, 0), {TokenKind::PIE, 0, 0}, add(5, 5)};
        sent.to_move = hex::Player::RED;
        sent.over = false;

        StateUpdate got;
        CHECK(roundTrip(makeStateUpdate(sent), Opcode::STATE_UPDATE, got), "stato: rileggibile");
        CHECK(got == sent, "stato: identico all'originale");
        CHECK(got.moves.size() == 3, "stato: tutte le mosse sono arrivate");
    }
    {
        // A finished match carries its outcome and end reason.
        StateUpdate sent;
        sent.board_size = 5;
        sent.moves = {add(1, 1)};
        sent.over = true;
        sent.winner = hex::Player::BLUE;
        sent.reason = hex::EndReason::RESIGN;

        StateUpdate got;
        CHECK(roundTrip(makeStateUpdate(sent), Opcode::STATE_UPDATE, got), "stato finale: rileggibile");
        CHECK(got.over && got.winner == hex::Player::BLUE, "stato finale: vincitore");
        CHECK(got.reason == hex::EndReason::RESIGN, "stato finale: motivo");
    }
    {
        // A match just begun: no moves, and that is correct.
        StateUpdate sent;
        StateUpdate got;
        CHECK(roundTrip(makeStateUpdate(sent), Opcode::STATE_UPDATE, got), "stato vuoto: rileggibile");
        CHECK(got.moves.empty(), "stato vuoto: nessuna mossa");
    }
    {
        // A declared count is the cheapest way to make a peer allocate memory, so it
        // is checked before reserving rather than after.
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::STATE_UPDATE)
               << static_cast<std::int32_t>(11)
               << static_cast<std::uint32_t>(4'000'000'000u);

        (void)readOpcode(packet);
        StateUpdate got;
        CHECK(!read(packet, got), "stato: conteggio smisurato rifiutato");
    }
    {
        // Plausible count but no moves: the packet is truncated.
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::STATE_UPDATE)
               << static_cast<std::int32_t>(11)
               << static_cast<std::uint32_t>(5);

        (void)readOpcode(packet);
        StateUpdate got;
        CHECK(!read(packet, got), "stato: conteggio che promette piu' del contenuto");
    }
    {
        // A move outside the board declared in the very same packet.
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::STATE_UPDATE)
               << static_cast<std::int32_t>(5)
               << static_cast<std::uint32_t>(1)
               << static_cast<std::uint8_t>(TokenKind::ADD)
               << static_cast<std::int32_t>(7) << static_cast<std::int32_t>(0);

        (void)readOpcode(packet);
        StateUpdate got;
        CHECK(!read(packet, got), "stato: mossa fuori dalla scacchiera dichiarata");
    }
    {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::STATE_UPDATE)
               << static_cast<std::int32_t>(11)
               << static_cast<std::uint32_t>(0)
               << static_cast<std::uint8_t>(0)     // to_move
               << true                             // over
               << static_cast<std::uint8_t>(0)     // winner
               << static_cast<std::uint8_t>(42);   // motivo inesistente

        (void)readOpcode(packet);
        StateUpdate got;
        CHECK(!read(packet, got), "stato: motivo di fine fuori enum rifiutato");
    }

    // --- ERROR_MSG ---
    {
        const ErrorMessage sent{ErrorCode::NOT_YOUR_TURN, describe(ErrorCode::NOT_YOUR_TURN)};
        ErrorMessage got;
        CHECK(roundTrip(makeError(sent), Opcode::ERROR_MSG, got), "errore: rileggibile");
        CHECK(got == sent, "errore: identico all'originale");
        CHECK(!got.text.empty(), "errore: il testo e' mostrabile");
    }
    {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(Opcode::ERROR_MSG)
               << static_cast<std::uint8_t>(0) << std::string("x");

        (void)readOpcode(packet);
        ErrorMessage got;
        CHECK(!read(packet, got), "errore: codice zero rifiutato");
    }
    {
        CHECK(describe(ErrorCode::SERVER_FULL) != describe(ErrorCode::ILLEGAL_MOVE),
              "errore: motivi diversi si leggono diversi");
    }

    // --- Messages are not confusable with one another ---
    {
        // Reading a packet as though it were another message must fail rather than
        // produce plausible data: that is what guards against desynchronisation.
        sf::Packet packet = makeMoveIntent(MoveIntent{add(2, 2)});
        const std::optional<Opcode> opcode = readOpcode(packet);
        CHECK(opcode == Opcode::MOVE_INTENT, "instradamento: opcode corretto");

        StateUpdate wrong;
        CHECK(!read(packet, wrong), "instradamento: letto come stato, viene rifiutato");
    }

    // --- The remote player follows the same rules as the local one ---
    {
        // The turn window and the validation live in the shared base class; what is
        // checked here is that a token becomes the right move and that nothing is
        // accepted out of turn.
        hexplay::DeferredPlayer player("Remoto");
        const hex::Situation start(hex::HexBoard(5), hex::Player::RED);

        const hex::Move placement{hex::MoveKind::ADD,
                                  hex::Action(hex::ActionKind::ADD, hex::Piece::RED_DISC, {2, 2})};

        CHECK(!player.submit(placement), "differito: fuori turno la mossa e' scartata");

        player.startMove(start);
        CHECK(player.isArmed(), "differito: la finestra e' aperta");
        CHECK(player.wouldAccept(placement), "differito: la mossa legale e' accettabile");
        CHECK(player.submit(placement), "differito: la mossa entra in coda");
        CHECK(!player.submit(placement), "differito: una seconda mossa non entra");

        const std::optional<hex::Move> taken = player.tryTakeMove();
        CHECK(taken == placement, "differito: il motore ritira la mossa giusta");
        CHECK(!player.isArmed(), "differito: la finestra si chiude alla consegna");
    }

    // --- The online match as the client sees it ---
    {
        // The client does not referee: it adopts the state that arrives and replays it.
        const MatchStart start{5, hex::Player::BLUE, "Avversario"};
        NetworkMatch match(start, "Io", loc);

        CHECK(match.boardSize() == 5, "partita: la scacchiera e' quella assegnata");
        CHECK(match.colour() == hex::Player::BLUE, "partita: il colore lo assegna il server");
        CHECK(match.opponentName() == "Avversario", "partita: il nome dell'avversario");
        CHECK(!match.isMyTurn(), "partita: il Blu non muove per primo");
        CHECK(!match.wouldAccept({0, 0}), "partita: fuori turno nessuna cella e' giocabile");

        StateUpdate update;
        update.board_size = 5;
        update.moves = {add(0, 0)};
        update.to_move = hex::Player::BLUE;

        CHECK(match.applyState(update), "partita: stato adottato");
        CHECK(match.moveCount() == 1, "partita: una mossa nella storia");
        CHECK(match.lastMove() == std::make_pair(0, 0), "partita: ultima mossa evidenziata");
        CHECK(match.isMyTurn(), "partita: ora tocca al Blu");
        CHECK(match.wouldAccept({2, 2}), "partita: una cella vuota e' giocabile");
        CHECK(!match.wouldAccept({0, 0}), "partita: la cella occupata non lo e'");
        CHECK(match.canSwap(), "partita: dopo la prima mossa lo scambio e' disponibile");
    }
    {
        // Resynchronisation: a state carrying more moves puts everything right, even
        // if the intermediate updates never arrived.
        NetworkMatch match({5, hex::Player::RED, "Altro"}, "Io", loc);

        StateUpdate jump;
        jump.board_size = 5;
        jump.moves = {add(0, 0), add(1, 1), add(2, 2), add(3, 3)};

        CHECK(match.applyState(jump), "risincronizzazione: salto in avanti adottato");
        CHECK(match.moveCount() == 4, "risincronizzazione: tutte le mosse recuperate");
        CHECK(match.isMyTurn(), "risincronizzazione: il turno e' coerente con la storia");
        CHECK(match.lastMove() == std::make_pair(3, 3), "risincronizzazione: ultima mossa giusta");
    }
    {
        // A state that does not describe this match is rejected in full, and the good
        // one from before stays in place.
        NetworkMatch match({5, hex::Player::RED, "Altro"}, "Io", loc);

        StateUpdate good;
        good.board_size = 5;
        good.moves = {add(0, 0)};
        CHECK(match.applyState(good), "rifiuto: si parte da uno stato buono");

        StateUpdate other_board;
        other_board.board_size = 11;
        CHECK(!match.applyState(other_board), "rifiuto: scacchiera diversa");

        StateUpdate impossible;
        impossible.board_size = 5;
        impossible.moves = {add(0, 0), add(0, 0)};   // due pedine sulla stessa cella
        CHECK(!match.applyState(impossible), "rifiuto: storia non rigiocabile");

        CHECK(match.moveCount() == 1, "rifiuto: lo stato precedente e' intatto");
        CHECK(match.lastMove() == std::make_pair(0, 0), "rifiuto: la posizione non e' cambiata");
    }
    {
        // The server declares the outcome: a win by forfeit leaves no trace on the
        // board, so replaying would not surface it.
        NetworkMatch match({5, hex::Player::RED, "Altro"}, "Io", loc);

        StateUpdate finished;
        finished.board_size = 5;
        finished.moves = {add(0, 0)};
        finished.over = true;
        finished.winner = hex::Player::RED;
        finished.reason = hex::EndReason::TIMEOUT;

        CHECK(match.applyState(finished), "esito: stato finale adottato");
        CHECK(match.isOver(), "esito: la partita risulta conclusa");
        CHECK(match.wonByLocal(), "esito: ha vinto il giocatore locale");
        CHECK(!match.isMyTurn(), "esito: a partita finita non tocca a nessuno");
        CHECK(!match.canSwap(), "esito: a partita finita non si scambia");
        CHECK(!match.wouldAccept({2, 2}), "esito: a partita finita non si gioca");
        CHECK(match.statusText().find("Hai vinto") != std::string::npos,
              "esito: la riga di stato lo dice");
    }
    {
        // The tokens the client sends carry no colour: the turn decides it on the
        // server, exactly as for a save file.
        const MoveToken placement = NetworkMatch::placementToken({4, 2});
        CHECK(placement.kind == TokenKind::ADD, "token: piazzamento");
        CHECK(placement.row == 4 && placement.col == 2, "token: coordinate del click");
        CHECK(NetworkMatch::swapToken().kind == TokenKind::PIE, "token: scambio");
    }

    {
        // A token resolves to the move the turn dictates, not the one the sender
        // might want: the colour never travels.
        const hex::Situation start(hex::HexBoard(5), hex::Player::BLUE);
        std::string error;

        const std::optional<hex::Move> move = hexsave::moveFromToken(start, add(1, 1), error);
        CHECK(move.has_value(), "token: tradotto in mossa");
        CHECK(move->action.piece == hex::Piece::BLUE_DISC, "token: il colore lo decide il turno");
    }
}
