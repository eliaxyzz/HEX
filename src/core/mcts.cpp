/**
 * @file mcts.cpp
 * @brief Monte Carlo tree search implementation for Hex.
 */

#include "core/mcts.h"
#include <utility>
#include <vector>
#include <cmath>
#include <optional>
#include <random>
#include <chrono>
#include <algorithm>
#include <exception>
#include <thread>

namespace hex {

    /**
     * @brief Lightweight board representation tuned for playout throughput.
     *
     * Disjoint-set over a linearised grid with four virtual edge nodes, so win
     * detection is O(1) and a placement costs an amortised union.
     */
    class FastHexState {
        /** @brief Board side length, read from the Situation. */
        int size = 0;

        /** @brief Number of real cells, size * size. */
        int cells = 0;

        // Disjoint-set forest: `cells` real cells (0 .. cells-1) followed by the
        // four virtual edge nodes.
        std::vector<int> parent;

        // Cell colour: 0 empty, 1 red, 2 blue, 3 walled.
        std::vector<int> color;

        /**
         * @brief Colour marking a walled cell.
         *
         * A walled cell must not stay at 0. Leaving it empty would break two things
         * silently: bridgeReply() looks for a free carrier with color[y] == 0 and
         * would answer a bridge by playing inside the black hole, and the invariant
         * "0 means playable" would hold everywhere but one spot.
         *
         * With a colour of its own the cell is neither owned nor free: it unions
         * with nothing, since checkAndUnion() compares against 1 or 2, it is never
         * picked as a carrier, and it never enters empty_cells, the only pool the
         * rollout draws from.
         */
        static constexpr int BLOCKED_COLOR = 3;

        // Playable cells, shuffled once per rollout. Walled cells never enter it,
        // so the rollout cannot reach them.
        std::vector<int> empty_cells;

        // Virtual nodes for the four board edges: Red wins by joining TOP to BOT,
        // Blue by joining LFT to RGT.
        int RED_TOP = 0;
        int RED_BOT = 0;
        int BLUE_LFT = 0;
        int BLUE_RGT = 0;

    public:
        /**
         * @brief Builds an empty state, to be filled by assignment.
         * @note Used as a reusable buffer: assigning a state of the same size reuses
         * the existing allocations instead of requesting new ones.
         */
        FastHexState() = default;

        /**
         * @brief Converts a Situation into the fast playout representation.
         * @note Board size is read from the position rather than assumed.
         */
        explicit FastHexState(const Situation& s)
            : size(s.getBoard().getSize()), cells(size * size),
              parent(static_cast<std::size_t>(cells) + 4),
              color(static_cast<std::size_t>(cells), 0),
              RED_TOP(cells), RED_BOT(cells + 1), BLUE_LFT(cells + 2), BLUE_RGT(cells + 3) {

            // Every node starts as its own root.
            for (std::size_t i = 0; i < parent.size(); ++i) parent[i] = static_cast<int>(i);

            auto board = s.getBoardView();
            empty_cells.reserve(static_cast<std::size_t>(cells));

            for (int r = 0; r < size; r++) {
                for (int c = 0; c < size; c++) {
                    const int idx = r * size + c;
                    const Piece p = board[r][c];

                    if (p == Piece::RED_DISC) place(idx, 1);
                    else if (p == Piece::BLUE_DISC) place(idx, 2);
                    else if (p == Piece::BLOCKED) color[idx] = BLOCKED_COLOR;
                    else empty_cells.push_back(idx);
                }
            }
        }

        /**
         * @brief Returns the root of the set containing `i`.
         * @note Path compression flattens the forest, so repeated lookups during a
         * rollout stay near constant time. Non-const by design, unlike HexBoard,
         * because this state is thread-private.
         */
        int findSet(int i) {
            if (parent[i] == i) return i;
            return parent[i] = findSet(parent[i]);
        }

        /**
         * @brief Merges the sets of two nodes, cells or virtual edges alike.
         */
        void unionSets(int i, int j) {
            const int rootI = findSet(i);
            const int rootJ = findSet(j);
            if (rootI != rootJ) parent[rootI] = rootJ;
        }

        /**
         * @brief Places a stone and unions it with its edges and matching neighbours.
         * @param idx Linear cell index, 0 .. cells-1.
         * @param pColor 1 for Red, 2 for Blue.
         */
        void place(const int idx, const int pColor) {
            color[idx] = pColor;
            const int r = idx / size;
            const int c = idx % size;

            // Virtual edges.
            if (pColor == 1) {
                if (r == 0) unionSets(idx, RED_TOP);
                if (r == size - 1) unionSets(idx, RED_BOT);
            } else {
                if (c == 0) unionSets(idx, BLUE_LFT);
                if (c == size - 1) unionSets(idx, BLUE_RGT);
            }

            // The six hexagonal neighbours, guarded so a linear index never wraps
            // onto the adjacent row.
            if (r > 0) checkAndUnion(idx, idx - size, pColor);
            if (r < size - 1) checkAndUnion(idx, idx + size, pColor);
            if (c > 0) checkAndUnion(idx, idx - 1, pColor);
            if (c < size - 1) checkAndUnion(idx, idx + 1, pColor);
            if (r > 0 && c < size - 1) checkAndUnion(idx, idx - size + 1, pColor);
            if (r < size - 1 && c > 0) checkAndUnion(idx, idx + size - 1, pColor);
        }

        /**
         * @brief Unions the cell with a neighbour only if both share a colour.
         */
        void checkAndUnion(int idx, int neighbor, int pColor) {
            if (color[neighbor] == pColor) unionSets(idx, neighbor);
        }

        /**
         * @brief The six neighbours in cyclic order around a cell.
         * @note Geometric order, each entry adjacent to the next. That property is
         * what makes bridge detection a single pass.
         */
        static constexpr int RING_DR[6] = {-1, -1,  0,  1,  1,  0};
        static constexpr int RING_DC[6] = { 0,  1,  1,  0, -1, -1};

        /**
         * @brief Returns the remaining carrier when `intruder` invades a bridge of
         * the given colour, -1 otherwise.
         *
         * Two stones forming a bridge are two steps apart and share exactly two
         * empty carrier cells. Seen from a carrier the test collapses: the two
         * bridge stones are neighbours at ring positions k and k+2, and the other
         * carrier is the neighbour between them at k+1.
         *
         * No board-wide bridge search is therefore needed, only one pass over the
         * ring of the cell just played: six read-only iterations, no allocation and
         * no disjoint-set lookup.
         */
        [[nodiscard]] int bridgeReply(const int intruder, const int myColor) const {
            const int r = intruder / size;
            const int c = intruder % size;

            for (int k = 0; k < 6; ++k) {
                const int k1 = (k + 1) % 6;
                const int k2 = (k + 2) % 6;

                const int ar = r + RING_DR[k],  ac = c + RING_DC[k];
                const int br = r + RING_DR[k2], bc = c + RING_DC[k2];
                if (ar < 0 || ac < 0 || ar >= size || ac >= size) continue;
                if (br < 0 || bc < 0 || br >= size || bc >= size) continue;

                // Both bridge endpoints must be mine.
                if (color[ar * size + ac] != myColor) continue;
                if (color[br * size + bc] != myColor) continue;

                // The other carrier is the neighbour sitting between them.
                const int yr = r + RING_DR[k1], yc = c + RING_DC[k1];
                if (yr < 0 || yc < 0 || yr >= size || yc >= size) continue;

                if (const int y = yr * size + yc; color[y] == 0) return y;
            }
            return -1;
        }

        /**
         * @brief Applies a move incrementally.
         *
         * Lets the search descend the tree without rebuilding anything, since a
         * child differs from its parent by a single stone.
         *
         * @return true if applied; false for moves the disjoint-set cannot express,
         * namely the pie swap, which recolours an existing stone while a union
         * cannot be undone.
         */
        bool applyMove(const Move& m) {
            if (m.kind != MoveKind::ADD) return false;

            const int idx = m.action.position.first * size + m.action.position.second;
            place(idx, m.action.piece == Piece::RED_DISC ? 1 : 2);
            return true;
        }

        /**
         * @brief Tests whether the given player has connected its two edges.
         * @note Rollouts only ask about the side that just moved: checking the
         * opponent as well would double the lookups for no information, since a move
         * cannot win the game for the player who did not make it.
         */
        bool hasConnected(const Player p) {
            return p == Player::RED ? findSet(RED_TOP) == findSet(RED_BOT)
                                    : findSet(BLUE_LFT) == findSet(BLUE_RGT);
        }

        /**
         * @brief Returns the winner in O(1), if either player is connected.
         */
        std::optional<Player> checkWin() {
            if (hasConnected(Player::RED)) return Player::RED;
            if (hasConnected(Player::BLUE)) return Player::BLUE;
            return std::nullopt;
        }

        /**
         * @brief Plays the position out at random until somebody connects.
         *
         * Shuffles the free cells once, then fills the board following that
         * permutation.
         *
         * @param turn Side to move at the start of the simulation.
         * @param save_bridges Answer bridge intrusions instead of moving at random.
         * @param rng Random stream of the calling tree. Not incidental: under root
         * parallelisation each tree needs its own stream, otherwise N trees would
         * replay identical simulations and cost N times as much for no extra
         * information.
         * @return The winner, or nullopt in the theoretical case of a full board.
         */
        std::optional<Player> rollout(Player turn, const bool save_bridges, std::mt19937& rng) {
            std::ranges::shuffle(empty_cells, rng);

            std::size_t next = 0;   // cursor into the permutation
            int last = -1;          // last cell played, for the bridge check

            while (true) {
                const int myColor = (turn == Player::RED) ? 1 : 2;
                int idx = -1;

                // Forced reply: the opponent just invaded a bridge.
                if (save_bridges && last >= 0) idx = bridgeReply(last, myColor);

                // Otherwise take the next free cell in the permutation. The cursor
                // skips cells filled while descending the tree as well as those
                // consumed out of order by a bridge reply.
                if (idx < 0) {
                    while (next < empty_cells.size() && color[empty_cells[next]] != 0) ++next;
                    if (next == empty_cells.size()) break;
                    idx = empty_cells[next++];
                }

                place(idx, myColor);
                if (hasConnected(turn)) return turn;

                last = idx;
                turn = opponent(turn);
            }
            return std::nullopt; // Unreachable in Hex: a draw is impossible
        }
    };


    /**
     * @brief One node of a search tree, owning its children.
     */
    struct Node {
        Situation situation;             ///< Game state at this node.
        Move move;                       ///< Move that led here.
        Node* parent;                    ///< Parent node, null at the root.
        std::vector<Node*> children;     ///< Expanded children, owned.
        std::vector<Move> untried_moves; ///< Legal moves not yet expanded.

        int visits = 0;                  ///< Times this node was visited.
        double wins = 0.0;               ///< Accumulated wins for player_just_moved.
        Player player_just_moved;        ///< Side that played `move`.

        /**
         * @brief Builds a node for the given state.
         * @note Situation is taken by value so a caller holding a temporary can move
         * it in and avoid a second board copy.
         */
        Node(Situation s, Move m, Node* p)
            : situation(std::move(s)), move(std::move(m)), parent(p),
              // to_move advances even on the move that ends the game, so the player
              // who just moved is always the opponent of the side to move.
              player_just_moved(opponent(situation.toMove())) {

            if (!situation.isOver()) untried_moves = situation.validMoves();
        }

        /** @brief Deletes the subtree rooted here. */
        ~Node() { for (Node* c : children) delete c; }

        [[nodiscard]] bool isTerminal() const { return situation.isOver(); }
        [[nodiscard]] bool isFullyExpanded() const { return untried_moves.empty(); }

        /**
         * @brief Computes the UCB1 score, balancing win rate against novelty.
         * @param log_total_visits Logarithm of the parent visit count, precomputed.
         * It depends only on the parent, and recomputing it here would repeat the
         * call once per child, up to 121 of them at the root.
         * @param c Exploration constant.
         */
        [[nodiscard]] double getUCB1(const double log_total_visits, const double c) const {
            if (visits == 0) return 1e9; // Unvisited nodes take priority
            return (wins / visits) + c * std::sqrt(log_total_visits / static_cast<double>(visits));
        }
    };

    /**
     * @brief Selection phase: descends to the child with the best UCB1 score.
     */
    Node* selectBestChild(const Node* node, const double c) {
        Node* best = nullptr;
        double best_value = -1.0;

        // One logarithm per node rather than one per child.
        const double log_total = std::log(std::max(1.0, static_cast<double>(node->visits)));

        for (Node* child : node->children) {
            if (const double ucb = child->getUCB1(log_total, c); ucb > best_value) {
                best_value = ucb;
                best = child;
            }
        }
        return best;
    }

    /**
     * @brief Expansion phase: adds one child for a random untried move.
     */
    Node* expand(Node* node, std::mt19937& rng) {
        const int max_idx = static_cast<int>(node->untried_moves.size()) - 1;
        std::uniform_int_distribution<int> dist(0, max_idx);
        const int idx = dist(rng);

        // Swap-and-pop: order in untried_moves is irrelevant, and this keeps the
        // removal O(1).
        Move m = node->untried_moves[idx];
        node->untried_moves[idx] = node->untried_moves.back();
        node->untried_moves.pop_back();
        Node* child = new Node(node->situation.next(m), m, node);
        node->children.push_back(child);
        return child;
    }

    /**
     * @brief Backpropagation phase: updates visits and wins up to the root.
     */
    void backpropagate(Node* node, const std::optional<Player> winner) {
        while (node) {
            node->visits++;
            // A node scores when the simulation was won by the player whose move
            // leads into it.
            if (node->player_just_moved == winner) node->wins += 1.0;
            node = node->parent;
        }
    }

    // --- Root parallelisation -----------------------------------------------

    namespace {

        /** @brief Search clock: monotonic, hence suitable for a deadline. */
        using Clock = std::chrono::steady_clock;

        /** @brief What a single tree hands back when it finishes. */
        struct TreeResult {
            /**
             * @brief Visit counts of the root moves, in canonical order.
             * @note Indexed like the list passed to growTree(), so merging two trees
             * is an element-wise sum with no move comparisons.
             */
            std::vector<long long> visits;

            /** @brief Iterations completed by this tree. */
            long long iterations = 0;
        };

        /**
         * @brief Builds the random stream for the given tree.
         *
         * std::random_device alone is not enough: on some implementations, MinGW
         * among them, it is deterministic, and identically seeded trees would
         * explore identical simulations, turning parallelisation into a pure cost
         * multiplier. Mixing in the clock and the tree index guarantees distinct
         * streams whatever the library does.
         */
        std::mt19937 makeRng(const unsigned index) {
            const auto now = static_cast<std::uint32_t>(
                Clock::now().time_since_epoch().count());

            std::seed_seq seed{std::random_device{}(), now, index};
            return std::mt19937(seed);
        }

        /** @brief Returns the index of a move in the canonical list, -1 if absent. */
        int indexOf(const std::vector<Move>& moves, const Move& m) {
            for (std::size_t i = 0; i < moves.size(); ++i) {
                if (moves[i] == m) return static_cast<int>(i);
            }
            return -1;
        }

        /**
         * @brief Grows one complete tree until the deadline.
         *
         * Everything it touches is private: the tree, the generator, the rollout
         * buffers. All it reads from the outside is the root position, the
         * configuration, the deadline and the stop token, each either immutable or
         * already safe for concurrent use. There is consequently nothing to lock,
         * which is why this function holds no mutex.
         */
        void growTree(const Situation& situation, const MCTSConfig& config,
                      const Clock::time_point deadline, const std::stop_token& stop,
                      const std::vector<Move>& root_moves, const unsigned index,
                      TreeResult& out) {
            out.visits.assign(root_moves.size(), 0);

            Node* root = new Node(situation, Move(), nullptr);
            std::mt19937 rng = makeRng(index);

            // Rollout state of the root position, built once.
            const FastHexState root_state(situation);

            // Buffer reused every iteration: assigning from a state of the same
            // size reuses its storage, so after the first pass the search loop
            // performs no allocation at all.
            FastHexState state;

            while (true) {
                // Cooperative cancellation, polled by every tree on every
                // iteration: a waiter blocks for the slowest thread to finish one
                // iteration, not for the deadline.
                if (stop.stop_requested()) break;
                if (Clock::now() >= deadline) break;

                Node* node = root;

                // The rollout state restarts at the root and descends with the tree,
                // one stone per edge, instead of being rebuilt at the leaf.
                state = root_state;
                bool incremental = true;

                // Selection: descend to a leaf or a partially expanded node.
                while (!node->isTerminal() && node->isFullyExpanded()) {
                    node = selectBestChild(node, config.exploration);
                    if (incremental && !state.applyMove(node->move)) incremental = false;
                }

                // Expansion: add a child where one is still available.
                if (!node->isTerminal()) {
                    node = expand(node, rng);
                    if (incremental && !state.applyMove(node->move)) incremental = false;
                }

                // Pie rule fallback: a swap is not expressible as a placement, so on
                // that branch the state is rebuilt from scratch. It affects at most
                // one edge of the tree, on the second ply of the game.
                if (!incremental) state = FastHexState(node->situation);

                // Simulation: play the position out over the disjoint-set state.
                std::optional<Player> winner = state.checkWin();
                if (!winner) winner = state.rollout(node->situation.toMove(),
                                                    config.save_bridges, rng);

                // Backpropagation.
                backpropagate(node, winner);
                ++out.iterations;
            }

            // Extract the root statistics before dropping the tree: they are the
            // only thing that outlives this thread.
            for (const Node* child : root->children) {
                if (const int i = indexOf(root_moves, child->move); i >= 0) {
                    out.visits[static_cast<std::size_t>(i)] = child->visits;
                }
            }

            delete root;
        }

        /**
         * @brief growTree() with exceptions contained.
         *
         * An exception escaping a std::thread body terminates the process. Losing a
         * single tree, to exhausted memory for instance, is survivable instead: the
         * others still produced statistics and the search decides on those.
         */
        void growTreeGuarded(const Situation& situation, const MCTSConfig& config,
                             const Clock::time_point deadline, const std::stop_token& stop,
                             const std::vector<Move>& root_moves, const unsigned index,
                             TreeResult& out) noexcept {
            try {
                growTree(situation, config, deadline, stop, root_moves, index, out);
            } catch (...) {
                out.visits.assign(root_moves.size(), 0);
                out.iterations = 0;
            }
        }

        /** @brief Returns how many trees to grow, per configuration and host. */
        unsigned threadCount(const MCTSConfig& config) {
            if (config.threads > 0) return std::min(config.threads, MAX_SEARCH_THREADS);

            // hardware_concurrency() reports 0 when it cannot tell; one tree is the
            // right answer there, not zero.
            const unsigned hw = std::thread::hardware_concurrency();
            return std::clamp(hw == 0 ? 1u : hw, 1u, MAX_SEARCH_THREADS);
        }
    }

    Move MCTSPlayer::getMove(const Situation& situation, std::stop_token stop,
                             SearchStats* stats) const {
        const auto start = Clock::now();

        const auto finish = [&](const unsigned threads, const long long iterations) {
            if (!stats) return;
            stats->iterations = iterations;
            stats->threads = threads;
            stats->elapsed_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        };

        // The root moves define the canonical order every tree reports against.
        const std::vector<Move> root_moves = situation.isOver() ? std::vector<Move>{}
                                                                : situation.validMoves();

        // Degenerate cases: no move means resignation, a single one needs no search.
        if (root_moves.empty()) {
            finish(0, 0);
            return {};
        }
        if (root_moves.size() == 1) {
            finish(0, 0);
            return root_moves[0];
        }

        const auto deadline = start + std::chrono::duration_cast<Clock::duration>(
                                          std::chrono::duration<double, std::milli>(
                                              config.time_limit_ms));

        const unsigned threads = threadCount(config);

        // One result slot per tree, each written by exactly one thread, so no
        // synchronisation is required.
        std::vector<TreeResult> results(threads);

        // The calling thread grows tree 0 rather than idling, so threads == 1
        // spawns nothing and degenerates to a plain single-threaded search.
        std::vector<std::thread> workers;
        workers.reserve(threads - 1);

        for (unsigned i = 1; i < threads; ++i) {
            workers.emplace_back([&, i] {
                growTreeGuarded(situation, config, deadline, stop, root_moves, i, results[i]);
            });
        }

        growTreeGuarded(situation, config, deadline, stop, root_moves, 0, results[0]);

        // Always join, cancellation included: no thread may touch this stack frame
        // once getMove() returns.
        for (std::thread& w : workers) w.join();

        // Merge: sum the visits move by move and keep the globally most visited
        // one. Same robust-child criterion as a single-tree search, applied to the
        // total: a move that convinces trees grown from different seeds is more
        // credible than one that convinces a single tree.
        std::vector<long long> total(root_moves.size(), 0);
        long long iterations = 0;

        for (const TreeResult& tree : results) {
            iterations += tree.iterations;

            // A tree lost to an exception reports nothing; skip it.
            if (tree.visits.size() != total.size()) continue;
            for (std::size_t i = 0; i < total.size(); ++i) total[i] += tree.visits[i];
        }

        finish(threads, iterations);

        // Under immediate cancellation no tree visited anything, so this falls back
        // on a legal move rather than an invented one.
        std::size_t best = 0;
        for (std::size_t i = 1; i < total.size(); ++i) {
            if (total[i] > total[best]) best = i;
        }

        return root_moves[best];
    }
}
