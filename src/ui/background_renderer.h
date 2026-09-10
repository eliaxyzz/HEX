/**
 * @file background_renderer.h
 * @brief Animated backdrop shared by the boardless screens.
 *
 * A hexagonal grid drifting slowly on the diagonal while individual cells breathe
 * independently. Not decoration for its own sake: a static background makes the
 * game itself look static, and the menu screens are the first thing anyone sees.
 *
 * ### A class rather than a function
 *
 * The backdrop holds one piece of state, elapsed time, but that time must keep
 * running across screen changes: menu, settings and lobby have to show the same
 * backdrop at the same point, or every transition would visibly restart it. It
 * therefore lives in the application context alongside the assets, and the screens
 * only draw it.
 *
 * ### One vertex batch
 *
 * The grid is a few hundred hexagons. Drawing them as that many sf::ConvexShape
 * objects would mean as many draw calls per frame; here every edge goes into a
 * single sf::VertexArray of lines submitted in one go. Per-cell opacity travels in
 * the vertex colours, so the breathing costs nothing extra.
 */

#ifndef BACKGROUND_RENDERER_H
#define BACKGROUND_RENDERER_H

#include <SFML/Graphics.hpp>

#include "core/hex_geometry.h"
#include "ui/sfml_theme.h"

namespace hexgui {

    /** @brief Side length of the decorative grid. */
    inline constexpr int BACKDROP_CELLS = 26;

    /** @brief Circumradius of the backdrop cells, in pixels. */
    inline constexpr float BACKDROP_RADIUS = 42.0f;

    /** @brief Draws the animated hexagonal backdrop. */
    class BackgroundRenderer {
    public:
        /** @brief Builds the backdrop with the given palette. */
        explicit BackgroundRenderer(const Theme& theme = Theme::dark());

        /** @brief Replaces the palette in use. */
        void setTheme(const Theme& t) { theme = t; }

        /**
         * @brief Advances the backdrop's clock.
         * @param dt Seconds elapsed since the previous frame.
         */
        void update(float dt);

        /**
         * @brief Draws the grid, filling the given area.
         * @param target Destination surface.
         * @param width View width, in pixels.
         * @param height View height, in pixels.
         */
        void draw(sf::RenderTarget& target, float width, float height);

        /** @brief Returns the seconds accumulated so far. */
        [[nodiscard]] float elapsedSeconds() const { return elapsed; }

        /** @brief Returns the edges actually drawn on the last frame. */
        [[nodiscard]] std::size_t edgeCount() const { return mesh.getVertexCount() / 2; }

    private:
        /** @brief Current palette. */
        Theme theme;

        /** @brief Accumulated time; drives both the drift and the breathing. */
        float elapsed = 0.0f;

        /** @brief Every grid edge, rebuilt each frame. */
        sf::VertexArray mesh{sf::PrimitiveType::Lines};
    };
}

#endif //BACKGROUND_RENDERER_H
