/**
 * @file game_controller.cpp
 * @brief Step-driven game engine implementation.
 */

#include "core/game_controller.h"

#include <chrono>
#include <exception>
#include <optional>
#include <thread>
#include <utility>

namespace hex {

    namespace {
        /** @brief Back-off between run() polls while a player is thinking. */
        constexpr auto POLL_INTERVAL = std::chrono::milliseconds(1);
    }

    GameController::GameController(const int size, AbstractPlayer& red, AbstractPlayer& blue,
                                   const int seconds_per_move)
        : sit(HexBoard(size), Player::RED), red(&red), blue(&blue),
          seconds_per_move(seconds_per_move) {
        history.push_back(sit);
    }

    GameController::GameController(HexBoard board, AbstractPlayer& red, AbstractPlayer& blue,
                                   const int seconds_per_move)
        : sit(std::move(board), Player::RED), red(&red), blue(&blue),
          seconds_per_move(seconds_per_move) {
        history.push_back(sit);
    }

    void GameController::addObserver(GameObserver& observer) {
        observers.push_back(&observer);
    }

    std::optional<GameResult> GameController::result() const {
        if (!sit.isOver()) return std::nullopt;
        return GameResult{*sit.winner(), sit.getEndReason()};
    }

    AbstractPlayer& GameController::playerFor(const Player p) const {
        return p == Player::RED ? *red : *blue;
    }

    void GameController::finishWithLoss(const Player loser, const EndReason reason) {
        sit = sit.endedBy(opponent(loser), reason);
        // Not a move: kept out of the history, but undo() must still discard it.
        ended_by_event = true;
        const GameResult res{*sit.winner(), sit.getEndReason()};
        notify([&](GameObserver& o) { o.onGameEnd(sit, res); });
    }

    bool GameController::claimTimeout(const Player loser) {
        if (sit.isOver()) return false;

        // Stop whoever was thinking: the answer is worthless now, and leaving the
        // request in flight risks destroying the player while it works.
        if (move_requested) {
            AbstractPlayer& current = playerFor(sit.toMove());
            current.abortMove();
            move_requested = false;

            const std::string name = current.getName();
            const Player mover = sit.toMove();
            notify([&](GameObserver& o) { o.onTimeout(sit, mover, name); });
        } else {
            const std::string name = playerFor(loser).getName();
            notify([&](GameObserver& o) { o.onTimeout(sit, loser, name); });
        }

        finishWithLoss(loser, EndReason::TIMEOUT);
        return true;
    }

    StepResult GameController::step() {
        if (!started) {
            started = true;
            notify([&](GameObserver& o) { o.onGameStart(sit); });
        }

        if (sit.isOver()) return StepResult::GAME_OVER;

        const Player mover = sit.toMove();
        AbstractPlayer& current = playerFor(mover);
        const std::string name = current.getName();

        // First step of the turn: announce it and ask the player to start thinking.
        if (!move_requested) {
            notify([&](GameObserver& o) { o.onTurnStart(sit, mover, name); });
            current.startMove(sit);
            turn_started_at = std::chrono::steady_clock::now();
            move_requested = true;
        }

        std::optional<Move> m;
        try {
            m = current.tryTakeMove();
        } catch (const std::exception& e) {
            const std::string what = e.what();
            move_requested = false;
            notify([&](GameObserver& o) { o.onPlayerError(sit, mover, name, what); });
            finishWithLoss(mover, EndReason::ILLEGAL_MOVE);
            return StepResult::PLAYED;
        }

        // Still deciding: never block, hand control back to the caller.
        if (!m) {
            if (seconds_per_move > 0) {
                const auto elapsed = std::chrono::steady_clock::now() - turn_started_at;
                if (elapsed >= std::chrono::seconds(seconds_per_move)) {
                    current.abortMove();
                    move_requested = false;
                    notify([&](GameObserver& o) { o.onTimeout(sit, mover, name); });
                    finishWithLoss(mover, EndReason::TIMEOUT);
                    return StepResult::PLAYED;
                }
            }
            return StepResult::WAITING;
        }

        move_requested = false;

        if (!sit.isValid(*m)) {
            notify([&](GameObserver& o) { o.onInvalidMove(sit, mover, *m, name); });
            finishWithLoss(mover, EndReason::ILLEGAL_MOVE);
            return StepResult::PLAYED;
        }

        const Situation before = sit;
        sit = sit.next(*m);
        history.push_back(sit);
        moves.push_back(*m);
        notify([&](GameObserver& o) { o.onMove(before, *m, sit); });

        if (sit.isOver()) {
            const GameResult res{*sit.winner(), sit.getEndReason()};
            notify([&](GameObserver& o) { o.onGameEnd(sit, res); });
        }

        return StepResult::PLAYED;
    }

    bool GameController::canUndo() const {
        return ended_by_event || history.size() > 1;
    }

    bool GameController::undo() {
        if (!canUndo()) return false;

        // Close any request in flight: after the undo the turn restarts from scratch.
        if (move_requested) {
            playerFor(sit.toMove()).abortMove();
            move_requested = false;
        }

        std::optional<Move> undone;

        if (ended_by_event) {
            // A referee decision is not in the history, so dropping the flag suffices.
            ended_by_event = false;
        } else {
            undone = moves.back();
            moves.pop_back();
            history.pop_back();
        }

        // Reinstating the stored Situation restores board, side to move, outcome,
        // end reason and pie rule flag in one assignment.
        sit = history.back();

        notify([&](GameObserver& o) { o.onUndo(sit, undone); });
        return true;
    }

    bool GameController::replay(const std::vector<Move>& sequence) {
        // Only on a match that has not started, with nobody thinking: rewriting the
        // history under a request in flight would apply its move to the wrong
        // position.
        if (!moves.empty() || ended_by_event || sit.isOver() || move_requested) return false;

        // Build aside and commit at the end, so a sequence rejected halfway leaves
        // the controller untouched.
        Situation replayed = history.front();

        std::vector<Situation> new_history;
        std::vector<Move> new_moves;
        new_history.reserve(sequence.size() + 1);
        new_moves.reserve(sequence.size());
        new_history.push_back(replayed);

        for (const Move& m : sequence) {
            // A finished game accepts no further move, and each move must be legal
            // in the position it is played into.
            if (replayed.isOver() || !replayed.isValid(m)) return false;

            replayed = replayed.next(m);
            new_history.push_back(replayed);
            new_moves.push_back(m);
        }

        sit = std::move(replayed);
        history = std::move(new_history);
        moves = std::move(new_moves);
        return true;
    }

    GameResult GameController::run() {
        for (;;) {
            switch (step()) {
                case StepResult::GAME_OVER:
                    return *result();
                case StepResult::WAITING:
                    std::this_thread::sleep_for(POLL_INTERVAL);
                    break;
                case StepResult::PLAYED:
                    break;
            }
        }
    }
}
