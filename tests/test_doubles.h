/**
 * @file test_doubles.h
 * @brief Support players and observers used across the suite.
 */

#ifndef HEX_TEST_DOUBLES_H
#define HEX_TEST_DOUBLES_H

#include "core/game_controller.h"
#include "core/player.h"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Test doubles: support players and observers.
 * @note The using directive is confined to this namespace, so including the header
 * does not drag `hex` into the includer's scope.
 */
namespace hextest {

using namespace hex;

// Slow but cooperative player: polls the stop token and stops as soon as asked.
class SlowPlayer : public AbstractPlayer {
public:
    std::string getName() override { return "Slow"; }
    Move getMoveFromSit(Situation s) override {
        const std::stop_token stop = currentStopToken();
        for (int i = 0; i < 500 && !stop.stop_requested(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return s.validMoves()[0];
    }
};

// Slow and uncooperative player: ignores the token. Documents the cost of doing so.
class StubbornPlayer : public AbstractPlayer {
public:
    std::string getName() override { return "Stubborn"; }
    Move getMoveFromSit(Situation s) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        return s.validMoves()[0];
    }
};

class ResignPlayer : public AbstractPlayer {
public:
    std::string getName() override { return "Resign"; }
    Move getMoveFromSit(Situation) override { return {}; } // Move() == RESIGN
};


// Observer recording the sequence of events it receives.
class RecordingObserver final : public GameObserver {
public:
    std::vector<std::string> events;
    int moves = 0;
    std::optional<GameResult> final_result;

    void onGameStart(const Situation&) override { events.emplace_back("start"); }
    void onTurnStart(const Situation&, Player, const std::string&) override { events.emplace_back("turn"); }
    void onMove(const Situation& before, const Move&, const Situation& after) override {
        events.emplace_back("move");
        ++moves;
        // `before` and `after` must genuinely be two different states.
        if (before.toMove() == after.toMove()) events.emplace_back("BUG:before==after");
    }
    void onInvalidMove(const Situation&, Player, const Move&, const std::string&) override { events.emplace_back("invalid"); }
    void onTimeout(const Situation&, Player, const std::string&) override { events.emplace_back("timeout"); }
    void onPlayerError(const Situation&, Player, const std::string&, const std::string&) override { events.emplace_back("error"); }
    void onGameEnd(const Situation&, const GameResult& r) override {
        events.emplace_back("end");
        final_result = r;
    }
    void onUndo(const Situation&, const std::optional<Move>& undone) override {
        events.emplace_back(undone ? "undo" : "undo-end");
    }
};

// Player that always proposes an illegal move, using the wrong colour.
class IllegalPlayer : public AbstractPlayer {
public:
    std::string getName() override { return "Illegal"; }
    Move getMoveFromSit(Situation) override {
        return {MoveKind::ADD, Action(ActionKind::ADD, Piece::BLUE_DISC, {0, 0})};
    }
};

// Player that throws instead of moving.
class ThrowingPlayer : public AbstractPlayer {
public:
    std::string getName() override { return "Throwing"; }
    Move getMoveFromSit(Situation) override { throw std::runtime_error("boom"); }
};


// Asynchronous player: computes nothing and waits for a move to be handed to it
// from outside. The same shape a GUI human player takes, where submit() is called
// from the click handler.
class DeferredPlayer : public AbstractPlayer {
public:
    int start_calls = 0;

    std::string getName() override { return "Deferred"; }

    // Never invoked: the asynchronous contract is overridden in full.
    Move getMoveFromSit(Situation) override { throw std::runtime_error("non usato"); }

    // The move may be delivered before or after startMove, and is consumed exactly
    // once, like a click that waits in the queue until the turn collects it.
    void startMove(const Situation& s) override { ++start_calls; current = s; }
    std::optional<Move> tryTakeMove() override {
        std::optional<Move> m = ready;
        ready.reset();
        return m;
    }
    void abortMove() override { ready.reset(); }

    /** @brief Delivers the move from outside, as a user click would. */
    void submit(const Move& m) { ready = m; }

    /** @brief Returns the position the move was requested for. */
    [[nodiscard]] const Situation& asked() const { return current; }

private:
    Situation current;
    std::optional<Move> ready;
};


// step() never blocks: a synchronous engine runs on a worker, so several calls may
// be needed before the move is ready. This helper does what run() does, but for a
// single turn.
inline StepResult stepUntilSettled(GameController& c) {
    // The cap stops a badly written test from turning into a silent hang.
    for (int i = 0; i < 20000; ++i) {
        const StepResult r = c.step();
        if (r != StepResult::WAITING) return r;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return StepResult::WAITING;
}

inline Move add(Player p, std::pair<int,int> pos) {
    return {MoveKind::ADD, Action(ActionKind::ADD, pieceOf(p), pos)};
}


}   // namespace hextest

#endif //HEX_TEST_DOUBLES_H
