/**
 * @file test_hit_test.cpp
 * @brief Pixel-to-cell conversion.
 */

#include "ui/console_renderer.h"
#include "core/game_controller.h"
#include "core/game_ruler.h"
#include "core/hex_geometry.h"
#include "ui/sfml_game_observer.h"
#include "ui/sfml_human_player.h"
#include "core/test_players.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <set>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "test_doubles.h"
#include "test_framework.h"

using namespace hex;
using namespace hextest;

void hit_test() {
    // --- From pixel to cell ---
    {
        using hexgui::HexLayout;
        using hexgui::Point;

        auto dist = [](const Point a, const Point b) {
            const float dx = a.x - b.x, dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        };
        auto towards = [](const Point from, const Point to, const float t) {
            return Point{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t};
        };

        // Exact round trip through the centre, for three board sizes.
        for (const int n : {5, 11, 13}) {
            const HexLayout l = HexLayout::fit(n, 900.0f, 800.0f, 56.0f);
            for (int r = 0; r < n; ++r)
                for (int c = 0; c < n; ++c)
                    CHECK_QUIET(l.cellAt(l.centreOf(r, c)) == std::make_pair(r, c));
        }
        CHECK(quiet_failures == 0, "hit test: cellAt(centreOf(r,c)) == (r,c) su board 5, 11 e 13");
        quiet_failures = 0;

        const HexLayout layout = HexLayout::fit(11, 900.0f, 800.0f, 56.0f);
        const hex::HexBoard board(11);
        const float S = layout.circumradius();

        // Points just inside each vertex stay in the cell.
        {
            for (int r = 0; r < 11; ++r) {
                for (int c = 0; c < 11; ++c) {
                    const Point centre = layout.centreOf(r, c);
                    for (const Point v : layout.verticesOf(r, c)) {
                        CHECK_QUIET(layout.cellAt(towards(centre, v, 0.95f)) == std::make_pair(r, c));
                    }
                }
            }
            CHECK(quiet_failures == 0, "hit test: un punto appena dentro un vertice resta nella cella");
            quiet_failures = 0;
        }

        // Points just past a vertex land either off the board or in a cell sharing
        // that vertex. Never in a distant cell.
        {
            for (int r = 0; r < 11; ++r) {
                for (int c = 0; c < 11; ++c) {
                    const Point centre = layout.centreOf(r, c);
                    for (const Point v : layout.verticesOf(r, c)) {
                        const auto hit = layout.cellAt(towards(centre, v, 1.10f));
                        if (!hit) continue;                       // fuori dalla scacchiera: legittimo
                        if (*hit == std::make_pair(r, c)) continue;
                        // Must be a direct neighbour whose centre touches the vertex.
                        bool adjacent = false;
                        for (const auto& a : board.getAdjacentPos({r, c})) if (a == *hit) adjacent = true;
                        CHECK_QUIET(adjacent);
                        CHECK_QUIET(std::abs(dist(layout.centreOf(hit->first, hit->second), v) - S) < 0.05f);
                    }
                }
            }
            CHECK(quiet_failures == 0, "hit test: oltre un vertice si finisce solo in una cella che lo condivide");
            quiet_failures = 0;
        }

        // Points around each side's midpoint: inside stays put, outside crosses to
        // exactly the neighbour beyond that side.
        {
            const float inr = layout.inradius();
            for (int r = 0; r < 11; ++r) {
                for (int c = 0; c < 11; ++c) {
                    const Point centre = layout.centreOf(r, c);
                    for (int k = 0; k < 6; ++k) {
                        const float a = 60.0f * static_cast<float>(k) * std::numbers::pi_v<float> / 180.0f;
                        const Point n_dir{std::cos(a), std::sin(a)};

                        const Point inside{centre.x + n_dir.x * inr * 0.95f, centre.y + n_dir.y * inr * 0.95f};
                        CHECK_QUIET(layout.cellAt(inside) == std::make_pair(r, c));

                        const Point outside{centre.x + n_dir.x * inr * 1.05f, centre.y + n_dir.y * inr * 1.05f};
                        if (const auto hit = layout.cellAt(outside)) {
                            CHECK_QUIET(*hit != std::make_pair(r, c));
                            bool adjacent = false;
                            for (const auto& adj : board.getAdjacentPos({r, c})) if (adj == *hit) adjacent = true;
                            CHECK_QUIET(adjacent);
                        }
                    }
                }
            }
            CHECK(quiet_failures == 0, "hit test: attraversando un lato si passa al vicino oltre quel lato");
            quiet_failures = 0;
        }

        // A point exactly on a border belongs to one of the cells sharing it, never
        // to none: a click must never fall into a gap.
        {
            for (int r = 0; r < 11; ++r) {
                for (int c = 0; c < 11; ++c) {
                    const Point centre = layout.centreOf(r, c);
                    for (int k = 0; k < 6; ++k) {
                        const float a = 60.0f * static_cast<float>(k) * std::numbers::pi_v<float> / 180.0f;
                        const Point edge{centre.x + std::cos(a) * layout.inradius(),
                                         centre.y + std::sin(a) * layout.inradius()};
                        CHECK_QUIET(layout.cellAt(edge).has_value());
                    }
                }
            }
            CHECK(quiet_failures == 0, "hit test: i punti esattamente sul bordo cadono sempre in una cella");
            quiet_failures = 0;
        }

        // Off the board: no cell.
        {
            CHECK(layout.cellAt({-500.0f, -500.0f}) == std::nullopt, "hit test: punto lontano -> nessuna cella");
            CHECK(layout.cellAt({5000.0f, 5000.0f}) == std::nullopt, "hit test: oltre l'angolo opposto -> nessuna cella");

            // Just outside the top vertex of cell (0,0): the rhombus corner.
            const Point top = layout.centreOf(0, 0);
            CHECK(layout.cellAt({top.x, top.y - S * 1.20f}) == std::nullopt,
                  "hit test: appena sopra l'angolo della scacchiera -> nessuna cella");

            // The margin between the window edge and the board is not clickable.
            CHECK(layout.cellAt({2.0f, 2.0f}) == std::nullopt, "hit test: il margine non e' cliccabile");
        }

        // Voronoi invariant: the cell returned really does have the nearest centre,
        // and it contains the point. Densely sampled over the whole board area.
        {
            const Point lo = layout.boundsMin();
            const Point hi = layout.boundsMax();
            int inside_hits = 0;

            for (int i = 0; i <= 120; ++i) {
                for (int j = 0; j <= 120; ++j) {
                    const Point p{lo.x + (hi.x - lo.x) * static_cast<float>(i) / 120.0f,
                                  lo.y + (hi.y - lo.y) * static_cast<float>(j) / 120.0f};
                    const auto hit = layout.cellAt(p);
                    if (!hit) continue;
                    ++inside_hits;

                    CHECK_QUIET(layout.contains(hit->first, hit->second, p));

                    const float d = dist(p, layout.centreOf(hit->first, hit->second));
                    for (int r = 0; r < 11; ++r)
                        for (int c = 0; c < 11; ++c)
                            CHECK_QUIET(d <= dist(p, layout.centreOf(r, c)) + 0.01f);
                }
            }
            CHECK(quiet_failures == 0,
                  "hit test: su 14641 punti la cella trovata contiene il punto ed e' la piu' vicina");
            CHECK(inside_hits > 8000, "hit test: il campionamento copre davvero la scacchiera");
            quiet_failures = 0;
        }

        // The conversion is scale independent, so it holds after a resize too.
        {
            const HexLayout small = HexLayout::fit(11, 400.0f, 360.0f, 10.0f);
            const HexLayout large = HexLayout::fit(11, 1920.0f, 1080.0f, 80.0f);
            for (const HexLayout& l : {small, large})
                for (int r = 0; r < 11; ++r)
                    for (int c = 0; c < 11; ++c)
                        CHECK_QUIET(l.cellAt(l.centreOf(r, c)) == std::make_pair(r, c));
            CHECK(quiet_failures == 0, "hit test: corretto anche dopo un ridimensionamento della finestra");
            quiet_failures = 0;
        }
    }
}
