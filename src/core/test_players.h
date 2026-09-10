/**
 * @file test_players.h
 * @brief Baseline players used for testing and benchmarking.
 */

#ifndef TEST_PLAYERS_H
#define TEST_PLAYERS_H

#include "core/player.h"
#include <random>
#include <algorithm>

namespace test_players {

    /**
     * @brief Plays a uniformly random legal move.
     *
     * Serves two purposes: as the minimum baseline, since a search losing to it is
     * a sign of a serious defect, and as a stress test feeding the engine
     * unpredictable positions.
     */
    class RandomPlayer : public hex::AbstractPlayer {
    public:
        std::string getName() override { return "Random"; }

        hex::Move getMoveFromSit(hex::Situation s) override {
            auto moves = s.validMoves();
            std::ranges::shuffle(moves, std::mt19937(std::random_device{}()));

            // Never resign while a real move is available.
            if (moves[0].kind == hex::MoveKind::RESIGN && moves.size() > 1) {
                return moves[1];
            }

            return moves[0];
        }
    };

    /**
     * @brief Random player with a one-step greedy lookahead.
     *
     * Plays an immediately winning move when one exists, otherwise falls back to a
     * uniformly random choice. Checks that the search defends against obvious
     * threats and converts a won position instead of drifting.
     */
    class SmartRandomPlayer : public hex::AbstractPlayer {
    public:
        std::string getName() override { return "SmartRandom"; }

        hex::Move getMoveFromSit(hex::Situation s) override {
            auto moves = s.validMoves();
            const hex::Player me = s.toMove();

            // One-step lookahead: take an immediate win if one is on the board.
            for (const auto& m : moves) {
                if (m.kind == hex::MoveKind::RESIGN) continue;

                if (s.next(m).winner() == me) {
                    return m;
                }
            }

            // No forced win available: fall back to a uniformly random move.
            std::ranges::shuffle(moves, std::mt19937(std::random_device{}()));

            if (moves[0].kind == hex::MoveKind::RESIGN && moves.size() > 1) {
                return moves[1];
            }

            return moves[0];
        }
    };
}
#endif //TEST_PLAYERS_H