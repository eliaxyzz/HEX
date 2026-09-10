/**
 * @file mcts.h
 * @brief Monte Carlo tree search engine interface.
 */

#ifndef MCTS_H
#define MCTS_H

#include "core/game.h"
#include "core/move.h"

#include <stop_token>

namespace hex {

    /**
     * @brief Upper bound on the number of parallel search trees.
     *
     * Each thread grows its own tree, so memory scales with the thread count. Past
     * a certain width the gain from root parallelisation flattens out, as the trees
     * start to agree, while the memory cost does not. The cap stops a many-core host
     * from spending RAM it gains no strength from.
     */
    inline constexpr unsigned MAX_SEARCH_THREADS = 16;

    /**
     * @brief Search engine configuration.
     */
    struct MCTSConfig {
        /** @brief Search budget in milliseconds. */
        double time_limit_ms = 9500.0;

        /** @brief Exploration constant of the UCB1 formula. */
        double exploration = 1.414;

        /**
         * @brief Enables bridge defence inside the playouts.
         *
         * A uniform playout moves at random even when the reply is forced. With this
         * on, an intrusion into a bridge is answered in the carrier cell instead of
         * being left to chance, which makes the simulated games less absurd and the
         * value estimate more reliable.
         *
         * @note Can be turned off to compare the two policies in self-play.
         */
        bool save_bridges = true;

        /**
         * @brief Number of parallel search trees.
         *
         * Zero means as many as the host supports, i.e.
         * std::thread::hardware_concurrency() capped by MAX_SEARCH_THREADS. An
         * explicit value gives reproducible results to a benchmark or to a test that
         * must measure the effect of parallelisation rather than inherit it.
         */
        unsigned threads = 0;
    };

    /**
     * @brief Instrumentation for a single search, used to measure the effect of
     * engine changes.
     */
    struct SearchStats {
        /** @brief Completed iterations: selection, expansion, playout, backpropagation. */
        long long iterations = 0;

        /** @brief Wall-clock duration of the search, in milliseconds. */
        double elapsed_ms = 0.0;

        /**
         * @brief Trees grown in parallel during this search.
         * @note Iterations are summed across all trees, so without this figure a
         * faster search is indistinguishable from a merely wider one.
         */
        unsigned threads = 1;

        /** @brief Returns the playout rate, in playouts per second. */
        [[nodiscard]] double playoutsPerSecond() const {
            return elapsed_ms > 0.0 ? static_cast<double>(iterations) * 1000.0 / elapsed_ms : 0.0;
        }
    };

    /**
     * @brief Monte Carlo tree search engine for Hex.
     *
     * Builds an asymmetric search tree from random simulations, using UCB1 to
     * balance exploration of rarely visited moves against exploitation of the
     * promising ones.
     *
     * ### Root parallelisation
     *
     * The search runs on several threads, each growing an entirely private tree
     * from the same position. At the deadline the root statistics are summed and
     * the globally most visited move is played.
     *
     * No tree is shared, so the search loop holds no lock at all: threads never wait
     * on each other and there is no structure for them to race on. All they share is
     * read-only material (the root position, the configuration, the deadline) plus
     * the std::stop_token, which is safe by construction.
     *
     * That is why this form of parallelism wins here. The alternative, one tree
     * shared across threads, needs a per-node lock or atomic statistics, and on
     * playouts lasting microseconds the synchronisation costs more than it returns.
     * The price paid instead is memory: N trees rather than one, hence
     * MAX_SEARCH_THREADS.
     */
    class MCTSPlayer {
    public:
        /**
         * @brief Builds the engine with the given configuration.
         * @param cfg Search budget, exploration constant and parallelism.
         */
        explicit MCTSPlayer(const MCTSConfig& cfg = {}) : config(cfg) {}

        /** @brief Returns the configuration in use. */
        [[nodiscard]] const MCTSConfig& getConfig() const { return config; }

        /**
         * @brief Computes the best move for the given position.
         *
         * Runs the four MCTS phases across parallel trees until the configured budget
         * expires or cancellation is requested through `stop`.
         *
         * Cancellation is cooperative: every thread polls the token on each
         * iteration, so the search stops promptly and still returns a legal move, the
         * best found so far. No search thread is alive once this returns, since all
         * of them are joined before the statistics are merged. The caller can
         * therefore wait for the search and destroy the engine straight afterwards.
         *
         * @param situation Current game state.
         * @param stop Cancellation token; the default one is never signalled.
         * @param stats Optional sink for iteration count and duration.
         * @return Move with the highest total visit count.
         * @note Backed by FastHexState, a disjoint-set board representation tuned for
         * playout throughput. Board size is read from the position, so any side
         * length works.
         */
        [[nodiscard]] Move getMove(const Situation& situation, std::stop_token stop = {},
                                   SearchStats* stats = nullptr) const;

    private:
        /** @brief Search parameters. */
        MCTSConfig config;
    };
}

#endif //MCTS_H
