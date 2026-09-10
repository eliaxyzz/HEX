/**
 * @file background_renderer.cpp
 * @brief Animated backdrop implementation.
 */

#include "ui/background_renderer.h"

#include <array>
#include <cmath>
#include <cstdint>

#include "ui/animation.h"

namespace hexgui {

    namespace {
        /** @brief The grid repeats after one step, so the drift wraps on it. */
        constexpr float DRIFT_STEP = 120.0f;

        /**
         * @brief Margin past the edge within which a cell is still drawn.
         * @note Must exceed the circumradius: a cell whose centre falls just outside
         * still has edges inside, and culling it would bite a notch out of the
         * window border.
         */
        constexpr float CULL_MARGIN = BACKDROP_RADIUS + 10.0f;

        /** @brief Returns the same colour at the given opacity, 0 to 1. */
        sf::Color withAlpha(const sf::Color c, const float k) {
            return {c.r, c.g, c.b, static_cast<std::uint8_t>(255.0f * k)};
        }
    }

    BackgroundRenderer::BackgroundRenderer(const Theme& theme) : theme(theme) {}

    void BackgroundRenderer::update(const float dt) {
        elapsed += dt;

        // The backdrop's clock feeds nothing but two periodic functions, so it wraps
        // on the common multiple of the two cycles. A long session then never loses
        // float precision precisely where the motion must stay smooth.
        constexpr float WRAP = 10000.0f;
        if (elapsed > WRAP) elapsed -= WRAP;
    }

    void BackgroundRenderer::draw(sf::RenderTarget& target, const float width, const float height) {
        // Diagonal drift wrapping on the grid pitch: the pattern moves indefinitely
        // without ever leaving the covered area.
        const float shift = std::fmod(elapsed * hexanim::BACKDROP_SPEED, DRIFT_STEP);

        const HexLayout grid(BACKDROP_CELLS, BACKDROP_RADIUS,
                             {-180.0f + shift, -180.0f + shift * 0.5f});

        mesh.clear();

        for (int r = 0; r < BACKDROP_CELLS; ++r) {
            for (int c = 0; c < BACKDROP_CELLS; ++c) {
                const Point centre = grid.centreOf(r, c);
                if (centre.x < -CULL_MARGIN || centre.x > width + CULL_MARGIN) continue;
                if (centre.y < -CULL_MARGIN || centre.y > height + CULL_MARGIN) continue;

                // The seed depends only on the grid position, not on the frame, so a
                // cell keeps its own phase as the pattern drifts.
                const float alpha = hexanim::backdropAlpha(elapsed, hexanim::cellSeed(r, c));
                const sf::Color colour = withAlpha(theme.cell_outline, alpha);

                const std::array<Point, 6> pts = grid.verticesOf(r, c);
                for (std::size_t i = 0; i < pts.size(); ++i) {
                    const Point& a = pts[i];
                    const Point& b = pts[(i + 1) % pts.size()];

                    mesh.append(sf::Vertex{{a.x, a.y}, colour});
                    mesh.append(sf::Vertex{{b.x, b.y}, colour});
                }
            }
        }

        target.draw(mesh);
    }
}
