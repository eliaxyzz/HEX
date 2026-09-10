/**
 * @file player.cpp
 * @brief Adapts the synchronous player contract to the asynchronous one.
 */

#include "core/player.h"

#include <chrono>
#include <exception>
#include <utility>

namespace hex {

    void AbstractPlayer::startMove(const Situation& situation) {
        // At most one worker per player: close any request still in flight.
        abortMove();

        // Fresh cancellation source: a stop_source cannot be re-armed.
        stop = std::stop_source{};

        // The channel is heap allocated so it outlives this stack frame.
        auto slot = std::make_shared<std::promise<Move>>();
        pending = slot->get_future();

        worker = std::thread([this, s = situation, slot]() mutable {
            try {
                slot->set_value(getMoveFromSit(std::move(s)));
            } catch (...) {
                try { slot->set_exception(std::current_exception()); } catch (...) {}
            }
        });
    }

    std::optional<Move> AbstractPlayer::tryTakeMove() {
        if (!pending.valid()) return std::nullopt;
        if (pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return std::nullopt;

        // The value is ready, so the worker has returned and the join is immediate.
        if (worker.joinable()) worker.join();

        // Consume the channel before get(): a player exception then propagates
        // exactly once and the request is closed either way.
        std::future<Move> done = std::move(pending);
        pending = {};
        return done.get();
    }

    void AbstractPlayer::abortMove() {
        if (worker.joinable()) {
            stop.request_stop();
            worker.join();
        }
        pending = {};
    }

    AbstractPlayer::~AbstractPlayer() {
        abortMove();
    }
}
