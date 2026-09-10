/**
 * @file sfml_renderer.h
 * @brief Graphical view: draws a HexBoard with SFML.
 */

#ifndef SFML_RENDERER_H
#define SFML_RENDERER_H

#include <SFML/Graphics.hpp>

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/board.h"
#include "core/hex_geometry.h"
#include "core/move.h"
#include "ui/sfml_theme.h"

namespace hexgui {

    /**
     * @brief What to add to the board's drawing on this frame.
     * @note Grouped into a struct because these are optional and accumulate over
     * time; passing them as parameters would make the signature unreadable.
     */
    struct BoardOverlay {
        /** @brief Cell of the last move, to be outlined. */
        std::optional<std::pair<int, int>> last_move;

        /** @brief Cell currently appearing, when an animation is running. */
        std::optional<std::pair<int, int>> appearing;

        /** @brief Animation progress, 0 to 1. */
        float appear_progress = 1.0f;

        /**
         * @brief Winning chain to emphasise.
         * @note When non-empty, every other cell is faded towards the background. The
         * fading is what makes the chain stand out, rather than a veil over the
         * board, which would dim the chain too.
         */
        std::span<const std::pair<int, int>> winning_path{};

        /**
         * @brief Seconds since the winning chain appeared.
         * @note Drives the glow travelling along it; ignored when the chain is empty.
         */
        float win_elapsed = 0.0f;

        /**
         * @brief Cell to point the player at, with a pulsing glow.
         * @note Used by the tutorial, where exactly one move is accepted and it has
         * to be indicated without spelling out its coordinates.
         */
        std::optional<std::pair<int, int>> hint;

        /** @brief Seconds the hint has been lit; drives the pulse. */
        float hint_elapsed = 0.0f;
    };

    /**
     * @brief Draws the hexagonal board onto an SFML target.
     */
    class SfmlBoardRenderer {
    public:
        /**
         * @brief Builds the renderer with the given layout and theme.
         * @param layout Grid geometry.
         * @param theme Colour palette.
         */
        /**
         * @param font Font for the labels and the status bar. May be null, in which
         * case the text is not drawn but the board still is.
         */
        SfmlBoardRenderer(const HexLayout& layout, const sf::Font* font,
                          const Theme& theme = Theme::dark());

        /** @brief Replaces the palette in use. */
        void setTheme(const Theme& t) { theme = t; }

        /** @brief Returns the palette in use. */
        [[nodiscard]] const Theme& getTheme() const { return theme; }

        /** @brief Updates the geometry, for instance after a resize. */
        void setLayout(const HexLayout& layout);

        /** @brief Returns the geometry in use. */
        [[nodiscard]] const HexLayout& getLayout() const { return layout; }

        /**
         * @brief Draws the board.
         * @param target Destination surface.
         * @param board Board to draw.
         * @param overlay Optional highlights and animation for this frame.
         */
        void draw(sf::RenderTarget& target, const hex::HexBoard& board,
                  const BoardOverlay& overlay = {});

        /**
         * @brief Draws the ghost stone under the pointer.
         *
         * A preview of the move a click would produce.
         *
         * @param target Destination surface.
         * @param cell Cell under the pointer.
         * @param piece Colour of the stone shown translucent.
         * @param elapsed Seconds the pointer has rested on a playable cell; makes the
         * opacity breathe instead of sitting flat.
         * @warning Draw it only where that click would really be accepted, or the
         * visual feedback and the input would say two different things.
         */
        void drawGhost(sf::RenderTarget& target, std::pair<int, int> cell, hex::Piece piece,
                       float elapsed = 0.0f);

        /**
         * @brief Draws a few lines of text, such as the status bar.
         *
         * Reuses the font already loaded for the labels; with none available it draws
         * nothing and the rest of the interface stays usable.
         *
         * @param target Destination surface.
         * @param lines Lines to write, top to bottom.
         * @param top_left Top-left corner of the text block.
         * @param char_size Character height in pixels.
         */
        void drawHud(sf::RenderTarget& target, const std::vector<std::string>& lines,
                     Point top_left, unsigned char_size = 18);

    private:
        /** @brief Current geometry. */
        HexLayout layout;

        /** @brief Current palette. */
        Theme theme;

        /** @brief Shape reused for every cell, reconfigured on each draw. */
        sf::ConvexShape cell;

        /**
         * @brief Accessibility symbol overlaid on a stone.
         * @note Reused like `cell`: allocating one polygon per stone per frame would
         * be wasteful on a 19x19 board.
         */
        sf::ConvexShape symbol;

        /**
         * @brief Geometry of the glow on the winning chain.
         * @note One sf::VertexArray for the whole effect: a handful of triangles in a
         * single draw call rather than dozens of shapes.
         */
        sf::VertexArray sweep{sf::PrimitiveType::Triangles};

        /** @brief Shared font, lent by the application. May be null. Non-owning. */
        const sf::Font* font = nullptr;

        /**
         * @brief Prepares the polygon for the given cell.
         * @note The vertices are relative to the centre and the position is applied
         * separately, so setScale() grows the cell about its own centre rather than
         * about the window origin, which is what the appearance animation needs.
         */
        void shapeCell(int row, int col, float scale = 1.0f);

        /**
         * @brief Draws a stone's geometric mark in colourblind mode: a triangle for
         * Red, a diamond for Blue.
         *
         * @param scale Follows the appearance animation, so the symbol grows with the
         * stone instead of snapping on top of it.
         * @param opacity 0 to 1, to accompany the fade.
         */
        void drawSymbol(sf::RenderTarget& target, int row, int col, hex::Piece piece,
                        float scale, float opacity);

        /**
         * @brief Draws the glow travelling along the winning chain.
         * @note Builds `sweep` and submits it in one go.
         */
        void drawWinSweep(sf::RenderTarget& target,
                          std::span<const std::pair<int, int>> path, float elapsed);

        /**
         * @brief Appends to `sweep` a radial glow centred on a cell.
         * @note A hexagonal fan, opaque at the centre and transparent at the rim. The
         * same shape serves the winning chain and the tutorial hint, and keeping it in
         * one place stops the two glows from diverging.
         */
        void appendGlow(Point centre, float radius, sf::Color colour, float intensity);

        /**
         * @brief Draws the directional edges: red top and bottom, blue on the sides.
         * @note Each border cell contributes the sides of its own hexagon that face
         * outwards.
         */
        void drawBorders(sf::RenderTarget& target, int board_size, bool dimmed = false);

        /** @brief Draws a thick segment along side `edge` of the cell. */
        void drawEdge(sf::RenderTarget& target, int row, int col, int edge, sf::Color color);

        /** @brief Draws the column letters and row numbers, when a font is available. */
        void drawLabels(sf::RenderTarget& target, int board_size);
    };
}

#endif //SFML_RENDERER_H
