/**
 * @file deferred_player.cpp
 * @brief Deferred player implementation.
 */

#include "core/deferred_player.h"

#include <stdexcept>
#include <utility>

namespace hexplay {

    DeferredPlayer::DeferredPlayer(std::string name) : name(std::move(name)) {}

    hex::Move DeferredPlayer::getMoveFromSit(hex::Situation) {
        throw std::logic_error("deferred player: use startMove/tryTakeMove");
    }

    void DeferredPlayer::startMove(const hex::Situation& situation) {
        current = situation;
        queued.reset();
        armed = true;

        // The engine decides whether the swap is available: when the pie rule is
        // playable, the corresponding move appears among the legal ones.
        swap_move.reset();
        for (const hex::Move& m : current.validMoves()) {
            if (m.kind == hex::MoveKind::PIE) {
                swap_move = m;
                break;
            }
        }
    }

    std::optional<hex::Move> DeferredPlayer::tryTakeMove() {
        if (!queued) return std::nullopt;

        // Deliver once and close the window: no further move is accepted until
        // the next turn starts.
        const std::optional<hex::Move> m = queued;
        queued.reset();
        armed = false;
        swap_move.reset();
        return m;
    }

    void DeferredPlayer::abortMove() {
        armed = false;
        queued.reset();
        swap_move.reset();
    }

    bool DeferredPlayer::wouldAccept(const hex::Move& move) const {
        // Outside the turn window the move is dropped, not buffered.
        if (!armed || queued) return false;

        // Occupied cell, off-board target or wrong turn: the engine must never
        // see any of them.
        return current.isValid(move);
    }

    bool DeferredPlayer::submit(const hex::Move& move) {
        if (!wouldAccept(move)) return false;

        queued = move;
        return true;
    }
}
