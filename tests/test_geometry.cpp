/**
 * @file test_geometry.cpp
 * @brief Hexagonal grid geometry.
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

void geometry() {
    // --- Hexagonal grid geometry ---
    {
        using hexgui::HexLayout;
        using hexgui::Point;

        auto dist = [](const Point a, const Point b) {
            const float dx = a.x - b.x, dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        };

        const HexLayout layout(11, 30.0f, {100.0f, 100.0f});

        // All six vertices lie on the circumcircle.
        {
            const auto v = layout.verticesOf(4, 6);
            const Point c = layout.centreOf(4, 6);
            for (const Point pt : v) CHECK_QUIET(std::abs(dist(c, pt) - 30.0f) < 0.01f);
            CHECK(quiet_failures == 0, "geometria: i 6 vertici sono a distanza S dal centro");
            quiet_failures = 0;
        }

        // A pointy-top cell measures sqrt(3)*S wide and 2*S tall.
        {
            const auto v = layout.verticesOf(0, 0);
            float min_x = v[0].x, max_x = v[0].x, min_y = v[0].y, max_y = v[0].y;
            for (const Point pt : v) {
                min_x = std::min(min_x, pt.x); max_x = std::max(max_x, pt.x);
                min_y = std::min(min_y, pt.y); max_y = std::max(max_y, pt.y);
            }
            CHECK(std::abs((max_x - min_x) - 30.0f * std::sqrt(3.0f)) < 0.01f,
                  "geometria: larghezza cella = sqrt(3)*S");
            CHECK(std::abs((max_y - min_y) - 60.0f) < 0.01f, "geometria: altezza cella = 2*S");
        }

        // The load-bearing test: the geometric neighbours must be exactly those the
        // engine treats as adjacent. A flat-top or offset formula would diverge here.
        {
            const hex::HexBoard board(11);
            const float spacing = layout.centreSpacing();

            for (int r = 0; r < 11; ++r) {
                for (int c = 0; c < 11; ++c) {
                    std::set<std::pair<int, int>> engine;
                    for (const auto& a : board.getAdjacentPos({r, c})) engine.insert(a);

                    std::set<std::pair<int, int>> geometric;
                    for (int r2 = 0; r2 < 11; ++r2) {
                        for (int c2 = 0; c2 < 11; ++c2) {
                            if (r2 == r && c2 == c) continue;
                            const float d = dist(layout.centreOf(r, c), layout.centreOf(r2, c2));
                            if (std::abs(d - spacing) < 0.01f) geometric.insert({r2, c2});
                        }
                    }
                    CHECK_QUIET(engine == geometric);
                }
            }
            CHECK(quiet_failures == 0,
                  "geometria: i vicini disegnati coincidono con getAdjacentPos su tutte le 121 celle");
            quiet_failures = 0;
        }

        // No pair of centres is closer than the pitch, so no cells overlap.
        {
            float min_d = 1e9f;
            for (int r = 0; r < 11; ++r)
                for (int c = 0; c < 11; ++c)
                    for (int r2 = 0; r2 < 11; ++r2)
                        for (int c2 = 0; c2 < 11; ++c2)
                            if (r != r2 || c != c2)
                                min_d = std::min(min_d, dist(layout.centreOf(r, c), layout.centreOf(r2, c2)));
            CHECK(std::abs(min_d - layout.centreSpacing()) < 0.01f,
                  "geometria: nessuna cella piu' vicina del passo fra centri");
        }

        // fit(): the board fits the area, honours the margin and is centred in it.
        for (const int n : {5, 11, 13}) {
            const HexLayout fitted = HexLayout::fit(n, 900.0f, 800.0f, 20.0f);
            const Point lo = fitted.boundsMin();
            const Point hi = fitted.boundsMax();

            CHECK_QUIET(lo.x >= 20.0f - 0.5f && lo.y >= 20.0f - 0.5f);
            CHECK_QUIET(hi.x <= 900.0f - 20.0f + 0.5f && hi.y <= 800.0f - 20.0f + 0.5f);
            // Centring: opposite margins match on at least one axis; on the other,
            // space is left over because the aspect ratios differ.
            const float slack_x = std::abs(lo.x - (900.0f - hi.x));
            const float slack_y = std::abs(lo.y - (800.0f - hi.y));
            CHECK_QUIET(slack_x < 0.5f && slack_y < 0.5f);
        }
        CHECK(quiet_failures == 0, "geometria: fit() entra nell'area, rispetta il margine ed e' centrata (n = 5, 11, 13)");
        quiet_failures = 0;

        // The scale is the largest possible: one of the two sides meets the margin.
        {
            const HexLayout fitted = HexLayout::fit(11, 900.0f, 800.0f, 20.0f);
            const float used_w = fitted.boundsMax().x - fitted.boundsMin().x;
            const float used_h = fitted.boundsMax().y - fitted.boundsMin().y;
            CHECK(std::abs(used_w - 860.0f) < 0.5f || std::abs(used_h - 760.0f) < 0.5f,
                  "geometria: fit() usa tutto lo spazio disponibile su un asse");
        }

        // Row 0 rises and column 0 runs right: the Hex rhombus, not a rectangle.
        {
            CHECK(layout.centreOf(0, 1).x > layout.centreOf(0, 0).x,
                  "geometria: la colonna cresce verso destra");
            CHECK(layout.centreOf(1, 0).y > layout.centreOf(0, 0).y,
                  "geometria: la riga cresce verso il basso");
            CHECK(layout.centreOf(1, 0).x > layout.centreOf(0, 0).x,
                  "geometria: ogni riga e' spostata di mezza cella (forma a rombo)");
        }
    }
}
