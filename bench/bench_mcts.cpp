/**
 * @file bench_mcts.cpp
 * @brief Measures the MCTS engine's playouts per second.
 *
 * Exists to tell a real optimisation from a guess. The figure to watch is
 * playout/s: the search budget is fixed, so more playouts in the same second means
 * an engine that sees further.
 *
 * Three positions are measured, because the cost per playout depends on how many
 * stones are already on the board: rebuilding the state costs in proportion to the
 * stones, playing a rollout costs in proportion to the empty cells.
 *
 *     hex_bench [milliseconds_per_search] [repetitions] [parallel_trees]
 *
 * The third argument fixes the number of root-parallelisation trees; omitted, the
 * engine uses as many as the host supports. It exists to compare a single-tree
 * search against a parallel one: without a baseline, "faster" means nothing.
 */

#include "core/game.h"
#include "core/mcts.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

    /** @brief One benchmark position with a readable name. */
    struct Position {
        std::string name;
        hex::Situation situation;
    };

    /**
     * @brief Builds a position by applying alternating moves to fixed cells.
     * @note Nothing is random: two runs of the benchmark start from the same board,
     * which is what makes two measurements comparable.
     */
    hex::Situation buildPosition(const int size, const int plies) {
        hex::Situation s(hex::HexBoard(size), hex::Player::RED);

        // Deterministic sequence filling the board in a lattice pattern, chosen to
        // avoid completing a connection before the requested ply count. The isOver()
        // check below is the backstop for the cases where it does anyway.
        int placed = 0;
        for (int r = 0; r < size && placed < plies; ++r) {
            for (int c = 0; c < size && placed < plies; ++c) {
                if ((r + c) % 3 == 0) continue;
                const hex::Move m{hex::MoveKind::ADD,
                                  hex::Action(hex::ActionKind::ADD, pieceOf(s.toMove()), {r, c})};
                if (!s.isValid(m)) continue;
                s = s.next(m);
                ++placed;
                if (s.isOver()) return s;
            }
        }
        return s;
    }

    void report(const std::string& label, const hex::SearchStats& st) {
        std::cout << "  " << std::left << std::setw(22) << label
                  << std::right << std::setw(10) << st.iterations << " playout"
                  << std::setw(12) << std::fixed << std::setprecision(0)
                  << st.playoutsPerSecond() << " playout/s\n";
    }
}

int main(const int argc, char** argv) {
    const double budget_ms = (argc > 1) ? std::atof(argv[1]) : 2000.0;
    const int repetitions = (argc > 2) ? std::atoi(argv[2]) : 3;
    const auto threads = (argc > 3) ? static_cast<unsigned>(std::atoi(argv[3])) : 0u;

    const std::vector<Position> positions{
        {"apertura (0 pedine)",   hex::Situation(hex::HexBoard(11), hex::Player::RED)},
        {"medio gioco (30)",      buildPosition(11, 30)},
        {"finale (70)",           buildPosition(11, 70)},
    };

    const hex::MCTSPlayer engine(hex::MCTSConfig{.time_limit_ms = budget_ms,
                                                .threads = threads});

    // A very short search reports how many trees the engine actually uses: with
    // zero requested the choice is its own, and the number must be read rather than
    // assumed.
    hex::SearchStats probe;
    (void)hex::MCTSPlayer(hex::MCTSConfig{.time_limit_ms = 1.0, .threads = threads})
        .getMove(positions.front().situation, {}, &probe);

    std::cout << "Benchmark MCTS - " << budget_ms << " ms per ricerca, "
              << repetitions << " ripetizioni, board 11x11, "
              << probe.threads << " alberi paralleli\n";

    double total = 0.0;
    int samples = 0;

    for (const Position& p : positions) {
        std::cout << "\n" << p.name
                  << (p.situation.isOver() ? "  [posizione gia' conclusa]" : "") << "\n";

        double sum = 0.0;
        for (int i = 0; i < repetitions; ++i) {
            hex::SearchStats st;
            (void)engine.getMove(p.situation, {}, &st);
            report("ripetizione " + std::to_string(i + 1), st);
            sum += st.playoutsPerSecond();
        }

        const double mean = sum / repetitions;
        std::cout << "  " << std::left << std::setw(22) << "media"
                  << std::right << std::setw(22) << std::fixed << std::setprecision(0)
                  << mean << " playout/s\n";

        total += mean;
        ++samples;
    }

    std::cout << "\nMEDIA COMPLESSIVA: " << std::fixed << std::setprecision(0)
              << (total / samples) << " playout/s\n";
    return 0;
}
