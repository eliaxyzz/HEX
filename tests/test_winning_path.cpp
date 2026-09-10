/**
 * @file test_winning_path.cpp
 * @brief Winning chain and animation curves.
 */

#include "ui/animation.h"
#include "core/board.h"
#include "core/game.h"
#include "core/winning_path.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
#include <vector>

#include "test_framework.h"

using namespace hex;
using namespace hexpath;

namespace {

    /** @brief Checks that the path is a legitimate chain for that player. */
    bool isValidChain(const HexBoard& board, const std::vector<std::pair<int, int>>& path,
                      const Player winner) {
        if (path.empty()) return false;

        const int n = board.getSize();
        const Piece target = pieceOf(winner);
        const bool red = (winner == Player::RED);

        // Every cell belongs to the winner.
        for (const auto& [r, c] : path)
            if (board.getPieceAtPos({r, c}) != target) return false;

        // Endpoints on the two correct edges.
        if (red) {
            if (path.front().first != 0 || path.back().first != n - 1) return false;
        } else {
            if (path.front().second != 0 || path.back().second != n - 1) return false;
        }

        // Consecutive cells are adjacent.
        for (std::size_t i = 1; i < path.size(); ++i) {
            const auto neighbours = board.getAdjacentPos(path[i - 1]);
            if (std::ranges::find(neighbours, path[i]) == neighbours.end()) return false;
        }

        // No cell is repeated.
        const std::set<std::pair<int, int>> unique(path.begin(), path.end());
        return unique.size() == path.size();
    }
}

void winning_path() {
    // --- Animation curves ---
    {
        CHECK(hexanim::progress(0.0f, 0.2f) == 0.0f, "animazione: all'inizio l'avanzamento e' zero");
        CHECK(hexanim::progress(0.1f, 0.2f) == 0.5f, "animazione: a meta' durata e' mezzo");
        CHECK(hexanim::progress(0.2f, 0.2f) == 1.0f, "animazione: a fine durata e' uno");
        CHECK(hexanim::progress(9.0f, 0.2f) == 1.0f, "animazione: oltre la durata resta uno");
        CHECK(hexanim::progress(0.1f, 0.0f) == 1.0f, "animazione: durata nulla e' gia' conclusa");

        CHECK(hexanim::easeOutCubic(0.0f) == 0.0f, "attenuazione: parte da zero");
        CHECK(hexanim::easeOutCubic(1.0f) == 1.0f, "attenuazione: arriva a uno");
        CHECK(hexanim::easeOutCubic(-1.0f) == 0.0f, "attenuazione: sotto zero viene limitata");
        CHECK(hexanim::easeOutCubic(2.0f) == 1.0f, "attenuazione: sopra uno viene limitata");

        // Ease-out: fast at first, so at half the time it is past halfway.
        CHECK(hexanim::easeOutCubic(0.5f) > 0.8f, "attenuazione: a meta' tempo e' gia' oltre l'80%");

        // --- Cyclic phase ---
        // The ghost's pulse and the chain's glow repeat endlessly; what matters is
        // that the cycle has neither gaps nor jumps.
        CHECK(hexanim::phase(0.0f, 2.0f) == 0.0f, "fase: parte da zero");
        CHECK(hexanim::phase(1.0f, 2.0f) == 0.5f, "fase: a meta' periodo e' mezzo");
        CHECK(hexanim::phase(2.0f, 2.0f) == 0.0f, "fase: a fine periodo ricomincia");
        CHECK(hexanim::phase(5.0f, 2.0f) == 0.5f, "fase: il ciclo si ripete identico");
        CHECK(hexanim::phase(-0.5f, 2.0f) == 0.75f, "fase: un tempo negativo resta dentro il ciclo");
        CHECK(hexanim::phase(1.0f, 0.0f) == 0.0f, "fase: un periodo nullo non divide per zero");

        // --- Ghost stone pulse ---
        // The preview must never vanish entirely: a cell going dark would read as
        // "not playable", the opposite of what it says.
        {
            constexpr float LOW = 1.0f - hexanim::GHOST_PULSE_DEPTH;
            bool inside = true;
            bool saw_low = false;
            bool saw_high = false;

            for (int i = 0; i <= 100; ++i) {
                const float t = hexanim::GHOST_PULSE_PERIOD * static_cast<float>(i) / 100.0f;
                const float v = hexanim::ghostPulse(t);
                if (v < LOW - 1e-4f || v > 1.0f + 1e-4f) inside = false;
                if (v < LOW + 0.05f) saw_low = true;
                if (v > 0.95f) saw_high = true;
            }

            CHECK(inside, "respiro: l'opacita' resta fra il minimo e il pieno");
            CHECK(saw_low && saw_high, "respiro: nel periodo tocca sia il minimo sia il pieno");
            CHECK(hexanim::ghostPulse(0.0f) > 0.99f,
                  "respiro: sulla cella appena raggiunta l'anteprima e' piena");

            // One cycle later the value is exactly the same: no jerk at the moment
            // the wave restarts.
            CHECK(hexanim::ghostPulse(hexanim::GHOST_PULSE_PERIOD) > 0.99f,
                  "respiro: il ciclo si chiude dove era cominciato");
        }

        // --- Glow along the winning chain ---
        {
            constexpr int CELLS = 6;

            // The head enters before the first link and leaves after the last, so
            // both ends light up fully instead of appearing half lit.
            CHECK(hexanim::sweepHead(0.0f, CELLS) < 0.0f,
                  "bagliore: parte prima del primo anello");
            CHECK(hexanim::sweepHead(hexanim::WIN_SWEEP_PERIOD * 0.999f, CELLS)
                      > static_cast<float>(CELLS - 1),
                  "bagliore: finisce oltre l'ultimo anello");
            CHECK(hexanim::sweepHead(1.0f, 0) == 0.0f,
                  "bagliore: una catena vuota non ha una testa da muovere");

            CHECK(hexanim::sweepIntensity(3, 3.0f) == 1.0f,
                  "bagliore: sotto la testa l'anello e' acceso del tutto");
            CHECK(hexanim::sweepIntensity(0, 3.0f) == 0.0f,
                  "bagliore: oltre la scia l'anello e' spento");
            CHECK(hexanim::sweepIntensity(3, 2.0f) > hexanim::sweepIntensity(3, 1.0f),
                  "bagliore: piu' la testa e' vicina, piu' l'anello e' acceso");
            CHECK(hexanim::sweepIntensity(2, 3.0f) == hexanim::sweepIntensity(4, 3.0f),
                  "bagliore: la scia e' simmetrica attorno alla testa");

            // Every link lights at least once per cycle: the glow travels the whole
            // chain rather than only its middle stretch.
            bool all_lit = true;
            for (int i = 0; i < CELLS; ++i) {
                float best = 0.0f;
                for (int k = 0; k <= 200; ++k) {
                    const float t = hexanim::WIN_SWEEP_PERIOD * static_cast<float>(k) / 200.0f;
                    best = std::max(best, hexanim::sweepIntensity(i, hexanim::sweepHead(t, CELLS)));
                }
                if (best < 0.99f) all_lit = false;
            }
            CHECK(all_lit, "bagliore: in un giro ogni anello della catena si accende del tutto");
        }

        // --- Pulse of the cell to touch, in the tutorial ---
        // Deeper than the ghost's pulse but never fully dark: the cell must stay
        // visible even at the low point of the cycle.
        {
            bool inside = true;
            float lowest = 1.0f;
            float highest = 0.0f;

            for (int i = 0; i <= 100; ++i) {
                const float t = hexanim::HINT_PULSE_PERIOD * static_cast<float>(i) / 100.0f;
                const float v = hexanim::hintPulse(t);
                if (v < 0.0f || v > 1.0f) inside = false;
                lowest = std::min(lowest, v);
                highest = std::max(highest, v);
            }

            CHECK(inside, "suggerimento: l'intensita' resta fra zero e uno");
            CHECK(lowest > 0.3f, "suggerimento: la cella non si spegne mai del tutto");
            CHECK(highest > 0.99f, "suggerimento: nel ciclo raggiunge il massimo");
            CHECK(lowest < hexanim::ghostPulse(hexanim::GHOST_PULSE_PERIOD / 2.0f),
                  "suggerimento: batte piu' marcato del respiro dell'anteprima");
        }

        // --- Animated backdrop ---
        {
            // The seed must depend only on the position: were it to change between
            // frames the backdrop would flicker rather than breathe.
            CHECK(hexanim::cellSeed(3, 7) == hexanim::cellSeed(3, 7),
                  "sfondo: la stessa cella riceve sempre lo stesso seme");
            CHECK(hexanim::cellSeed(3, 7) != hexanim::cellSeed(7, 3),
                  "sfondo: righe e colonne scambiate danno semi diversi");

            bool in_range = true;
            for (int r = 0; r < 26; ++r) {
                for (int c = 0; c < 26; ++c) {
                    const float s = hexanim::cellSeed(r, c);
                    if (s < 0.0f || s >= 1.0f) in_range = false;
                }
            }
            CHECK(in_range, "sfondo: ogni seme cade in [0, 1)");

            // Different cells must sit at different points of the cycle: the phase
            // spread is what gives depth, cells in unison are a blink.
            const float a = hexanim::backdropAlpha(0.0f, hexanim::cellSeed(1, 1));
            const float b = hexanim::backdropAlpha(0.0f, hexanim::cellSeed(4, 9));
            CHECK(a != b, "sfondo: due celle non respirano all'unisono");

            bool bounded = true;
            for (int i = 0; i <= 120; ++i) {
                const float t = hexanim::BACKDROP_PULSE_PERIOD * static_cast<float>(i) / 60.0f;
                const float v = hexanim::backdropAlpha(t, 0.37f);
                if (v < hexanim::BACKDROP_MIN_ALPHA - 1e-4f
                    || v > hexanim::BACKDROP_MAX_ALPHA + 1e-4f) bounded = false;
            }
            CHECK(bounded, "sfondo: l'opacita' resta dentro i limiti dichiarati");
        }

        // Monotonic: a stone must never shrink while appearing.
        float previous = -1.0f;
        for (int i = 0; i <= 100; ++i) {
            const float v = hexanim::easeOutCubic(static_cast<float>(i) / 100.0f);
            CHECK_QUIET(v >= previous);
            previous = v;
        }
        CHECK(quiet_failures == 0, "attenuazione: cresce sempre, non torna mai indietro");
        quiet_failures = 0;

        // Never exceeds 1: a stone that overshoots overlaps its neighbours.
        for (int i = 0; i <= 100; ++i)
            CHECK_QUIET(hexanim::easeOutCubic(static_cast<float>(i) / 100.0f) <= 1.0f);
        CHECK(quiet_failures == 0, "attenuazione: non supera mai la dimensione piena");
        quiet_failures = 0;
    }

    // --- No win, no path ---
    {
        const HexBoard empty(11);
        CHECK(winningPath(empty, Player::RED).empty(), "percorso: scacchiera vuota, nessuna catena");
        CHECK(winningPath(empty, Player::BLUE).empty(), "percorso: nemmeno per il Blu");
        CHECK(winningPath(HexBoard(), Player::RED).empty(), "percorso: scacchiera di lato zero gestita");
    }
    {
        // An almost complete chain, missing its last cell.
        HexBoard b(5);
        for (int r = 0; r < 4; ++r) b.addPiece(Piece::RED_DISC, {r, 2});
        CHECK(!b.checkWin(Player::RED), "percorso: il motore conferma che non ha vinto");
        CHECK(winningPath(b, Player::RED).empty(), "percorso: catena incompleta, nessun risultato");
    }

    // --- Red's vertical win ---
    {
        HexBoard b(5);
        for (int r = 0; r < 5; ++r) b.addPiece(Piece::RED_DISC, {r, 2});

        const auto path = winningPath(b, Player::RED);
        CHECK(!path.empty(), "percorso: il Rosso ha una catena");
        CHECK(path.size() == 5, "percorso: cinque celle su una board 5x5");
        CHECK(isValidChain(b, path, Player::RED), "percorso: catena valida dall'alto al basso");
        CHECK(path.front().first == 0, "percorso: parte dal bordo superiore");
        CHECK(path.back().first == 4, "percorso: arriva al bordo inferiore");
        CHECK(winningPath(b, Player::BLUE).empty(), "percorso: il Blu non ha nessuna catena");
    }

    // --- Blue's horizontal win ---
    {
        HexBoard b(5);
        for (int c = 0; c < 5; ++c) b.addPiece(Piece::BLUE_DISC, {2, c});

        const auto path = winningPath(b, Player::BLUE);
        CHECK(isValidChain(b, path, Player::BLUE), "percorso: catena valida da sinistra a destra");
        CHECK(path.front().second == 0, "percorso: parte dal bordo sinistro");
        CHECK(path.back().second == 4, "percorso: arriva al bordo destro");
    }

    // --- The path is the shortest one, not the blob ---
    {
        // A straight column plus a large useless cluster attached to one side.
        HexBoard b(7);
        for (int r = 0; r < 7; ++r) b.addPiece(Piece::RED_DISC, {r, 3});
        for (int r = 1; r < 6; ++r) {
            b.addPiece(Piece::RED_DISC, {r, 4});
            b.addPiece(Piece::RED_DISC, {r, 5});
        }

        const auto path = winningPath(b, Player::RED);
        CHECK(isValidChain(b, path, Player::RED), "percorso: catena valida anche con pedine di contorno");
        CHECK(path.size() == 7, "percorso: sceglie le 7 celle della colonna, non le 17 del gruppo");
        CHECK(b.getPosByPiece(Piece::RED_DISC).size() == 17, "percorso: le pedine in eccesso ci sono davvero");
    }
    {
        // Zig-zag path: the shortest one is not a straight line.
        HexBoard b(5);
        const std::pair<int, int> chain[]{{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}};
        for (const auto& p : chain) b.addPiece(Piece::RED_DISC, p);
        // A side branch that does not reach the opposite edge.
        b.addPiece(Piece::RED_DISC, {1, 1});
        b.addPiece(Piece::RED_DISC, {1, 2});

        const auto path = winningPath(b, Player::RED);
        CHECK(isValidChain(b, path, Player::RED), "percorso: il ramo morto non lo confonde");
        CHECK(path.size() == 5, "percorso: il ramo morto non viene incluso");
    }

    // --- Two possible chains: a valid and minimal one is chosen ---
    {
        HexBoard b(5);
        for (int r = 0; r < 5; ++r) {
            b.addPiece(Piece::RED_DISC, {r, 0});
            b.addPiece(Piece::RED_DISC, {r, 4});
        }

        const auto path = winningPath(b, Player::RED);
        CHECK(isValidChain(b, path, Player::RED), "percorso: con due catene ne restituisce una valida");
        CHECK(path.size() == 5, "percorso: e comunque la piu' corta");
    }

    // --- Agreement with the engine on a genuinely played match ---
    {
        // A match built by hand up to Red's connection.
        Situation s(HexBoard(11), Player::RED);
        for (int r = 0; r < 10; ++r) {
            s = s.next({MoveKind::ADD, Action(ActionKind::ADD, Piece::RED_DISC, {r, 3})});
            s = s.next({MoveKind::ADD, Action(ActionKind::ADD, Piece::BLUE_DISC, {r, 7})});
        }
        s = s.next({MoveKind::ADD, Action(ActionKind::ADD, Piece::RED_DISC, {10, 3})});

        CHECK(s.isOver() && s.winner() == Player::RED, "percorso: il motore dichiara vincitore il Rosso");

        const auto path = winningPath(s.getBoard(), Player::RED);
        CHECK(isValidChain(s.getBoard(), path, Player::RED),
              "percorso: la catena trovata e' coerente con la vittoria del motore");
        CHECK(path.size() == 11, "percorso: undici celle su una board 11x11");
    }
    {
        // The loser has no chain, even once the match has ended.
        HexBoard b(5);
        for (int r = 0; r < 5; ++r) b.addPiece(Piece::RED_DISC, {r, 2});
        b.addPiece(Piece::BLUE_DISC, {0, 0});
        b.addPiece(Piece::BLUE_DISC, {0, 1});

        CHECK(winningPath(b, Player::BLUE).empty(), "percorso: nessuna catena per chi ha perso");
    }
}
