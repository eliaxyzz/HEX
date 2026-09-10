/**
 * @file network_match.cpp
 * @brief Client-side online match implementation.
 */

#include "network/network_match.h"

#include <utility>

#include "core/save_format.h"

namespace hexnet {

    namespace {
        /** @brief Returns the localized colour name, for the status line. */
        const std::string& colourName(const hexui::LocalizationManager& loc, const hex::Player p) {
            return loc.text(p == hex::Player::RED ? hexui::StringKey::COLOUR_RED
                                                  : hexui::StringKey::COLOUR_BLUE);
        }

        /** @brief Returns the localized description of an end reason. */
        const std::string& reasonText(const hexui::LocalizationManager& loc, const hex::EndReason r) {
            switch (r) {
                case hex::EndReason::CONNECTION:   return loc.text(hexui::StringKey::REASON_CONNECTION);
                case hex::EndReason::RESIGN:       return loc.text(hexui::StringKey::REASON_RESIGN);
                case hex::EndReason::TIMEOUT:      return loc.text(hexui::StringKey::REASON_TIMEOUT);
                case hex::EndReason::ILLEGAL_MOVE: return loc.text(hexui::StringKey::REASON_ILLEGAL);
                default:                           return loc.text(hexui::StringKey::REASON_UNKNOWN);
            }
        }
    }

    NetworkMatch::NetworkMatch(const MatchStart& start, std::string local_name,
                               const hexui::LocalizationManager& loc)
        : loc(loc),
          board_size(start.board_size),
          my_colour(start.colour),
          local(std::move(local_name)),
          opponent(start.opponent),
          current(hex::HexBoard(start.board_size), hex::Player::RED) {}

    bool NetworkMatch::applyState(const StateUpdate& update) {
        // An update for a different board size does not concern this match.
        if (update.board_size != board_size) return false;

        // Replay the history through the same function that validates a save, so
        // every move passes Situation::isValid and an inconsistent history is
        // rejected instead of producing an invented position.
        hexsave::SaveData data;
        data.board_size = board_size;
        data.moves = update.moves;

        std::string error;
        const std::optional<std::vector<hex::Move>> moves = hexsave::materialise(data, error);
        if (!moves) return false;

        // Build aside and commit at the end, so an update rejected halfway leaves
        // the match untouched.
        hex::Situation rebuilt(hex::HexBoard(board_size), hex::Player::RED);
        std::optional<std::pair<int, int>> highlight;

        for (const hex::Move& m : *moves) {
            rebuilt = rebuilt.next(m);
            if (m.kind == hex::MoveKind::ADD) highlight = m.action.position;
        }

        current = rebuilt;
        played = moves->size();
        last = highlight;

        // The server declares the outcome: a win by timeout or forfeit leaves no
        // trace in the history, so replaying it would not surface it.
        over = update.over;
        victor = update.winner;
        end_reason = update.reason;

        return true;
    }

    const std::string& NetworkMatch::nameOf(const hex::Player p) const {
        return p == my_colour ? local : opponent;
    }

    bool NetworkMatch::isMyTurn() const {
        return !over && !current.isOver() && current.toMove() == my_colour;
    }

    bool NetworkMatch::canSwap() const {
        if (!isMyTurn()) return false;

        // The engine decides whether the swap is available, not the interface.
        for (const hex::Move& m : current.validMoves()) {
            if (m.kind == hex::MoveKind::PIE) return true;
        }
        return false;
    }

    bool NetworkMatch::wouldAccept(const std::pair<int, int> pos) const {
        if (!isMyTurn()) return false;

        const hex::Move m{hex::MoveKind::ADD,
                          hex::Action(hex::ActionKind::ADD, pieceOf(my_colour), pos)};
        return current.isValid(m);
    }

    hexsave::MoveToken NetworkMatch::placementToken(const std::pair<int, int> pos) {
        return {hexsave::TokenKind::ADD, pos.first, pos.second};
    }

    hexsave::MoveToken NetworkMatch::swapToken() {
        return {hexsave::TokenKind::PIE, 0, 0};
    }

    std::string NetworkMatch::statusText() const {
        if (over) {
            const std::string who = wonByLocal()
                ? loc.text(hexui::StringKey::NET_YOU_WON)
                : nameOf(victor) + " " + loc.text(hexui::StringKey::NET_HAS_WON);
            return who + "  -  " + reasonText(loc, end_reason);
        }

        if (isMyTurn()) {
            return loc.text(hexui::StringKey::NET_YOUR_TURN)
                 + "  (" + colourName(loc, my_colour) + ")";
        }

        // The member name shadows the free function; the free one is meant here.
        return loc.text(hexui::StringKey::NET_WAIT_FOR) + " " + opponent
             + "  (" + colourName(loc, hex::opponent(my_colour)) + ")";
    }
}
