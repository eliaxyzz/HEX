/**
 * @file sfml_renderer.cpp
 * @brief Graphical view implementation.
 */

#include "ui/sfml_renderer.h"

#include "ui/animation.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace hexgui {

    namespace {
        /**
         * @brief Sides of a pointy-top hexagon, indexed like its vertices.
         *
         * Side k joins vertices k and k+1. With the vertex order HexLayout::verticesOf
         * uses (30 degrees + 60*k, y downwards) the sides map to the six neighbours as
         * follows:
         *
         *   0 -> (r+1, c)     bottom-right      3 -> (r-1, c)     top-left
         *   1 -> (r+1, c-1)   bottom-left       4 -> (r-1, c+1)   top-right
         *   2 -> (r, c-1)     left              5 -> (r, c+1)     right
         */
        constexpr int EDGE_DOWN_RIGHT = 0;
        constexpr int EDGE_DOWN_LEFT  = 1;
        constexpr int EDGE_LEFT       = 2;
        constexpr int EDGE_UP_LEFT    = 3;
        constexpr int EDGE_UP_RIGHT   = 4;
        constexpr int EDGE_RIGHT      = 5;

        sf::Vector2f toSfml(const Point p) { return {p.x, p.y}; }

        /** @brief Blends two colours; t = 0 returns `a`, t = 1 returns `b`. */
        sf::Color blend(const sf::Color a, const sf::Color b, const float t) {
            const auto mix = [t](const std::uint8_t x, const std::uint8_t y) {
                return static_cast<std::uint8_t>(static_cast<float>(x)
                                                 + (static_cast<float>(y) - static_cast<float>(x)) * t);
            };
            return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
        }

        /** @brief How far cells outside the winning chain are dimmed. */
        constexpr float DIM_AMOUNT = 0.72f;

        /** @brief Returns the same colour with its opacity scaled by `k`, 0 to 1. */
        sf::Color fade(const sf::Color c, const float k) {
            const float a = static_cast<float>(c.a) * std::clamp(k, 0.0f, 1.0f);
            return {c.r, c.g, c.b, static_cast<std::uint8_t>(a)};
        }

        /** @brief Tests whether the cell appears in the list. */
        bool listed(const std::span<const std::pair<int, int>> cells, const int r, const int c) {
            for (const auto& [pr, pc] : cells) if (pr == r && pc == c) return true;
            return false;
        }
    }

    SfmlBoardRenderer::SfmlBoardRenderer(const HexLayout& layout, const sf::Font* font,
                                         const Theme& theme)
        : layout(layout), theme(theme), font(font) {
        cell.setPointCount(6);
        symbol.setPointCount(4);   // riportata a 3 o 4 punti a seconda della pedina
    }

    void SfmlBoardRenderer::setLayout(const HexLayout& l) { layout = l; }

    void SfmlBoardRenderer::shapeCell(const int row, const int col, const float scale) {
        const Point centre = layout.centreOf(row, col);
        const std::array<Point, 6> pts = layout.verticesOf(row, col);

        // Vertices relative to the centre: setPosition() applies the placement, so
        // scaling acts about the cell rather than about the window origin.
        for (std::size_t i = 0; i < pts.size(); ++i) {
            cell.setPoint(i, sf::Vector2f{pts[i].x - centre.x, pts[i].y - centre.y});
        }

        cell.setPosition(toSfml(centre));
        cell.setScale({scale, scale});
    }

    void SfmlBoardRenderer::draw(sf::RenderTarget& target, const hex::HexBoard& board,
                                 const BoardOverlay& overlay) {
        const int n = board.getSize();
        const auto cells = board.getBoardView();

        // The outline grows outwards, so keeping it thin stops adjacent cells from
        // eating into each other on a small board.
        const float thickness = std::max(1.0f, layout.circumradius() * theme.outline_thickness);
        const bool dimming = !overlay.winning_path.empty();

        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n; ++c) {
                const bool appearing = overlay.appearing
                                    && overlay.appearing->first == r && overlay.appearing->second == c;

                // An appearing stone grows from its own centre.
                const float grow = appearing ? std::max(0.02f, overlay.appear_progress) : 1.0f;
                shapeCell(r, c, grow);

                sf::Color fill;
                switch (cells[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]) {
                    case hex::Piece::RED_DISC:  fill = theme.piece_red;    break;
                    case hex::Piece::BLUE_DISC: fill = theme.piece_blue;   break;
                    case hex::Piece::BLOCKED:   fill = theme.cell_blocked; break;
                    default:                    fill = theme.cell_empty;   break;
                }

                sf::Color outline = theme.cell_outline;
                const bool highlighted = overlay.last_move
                                      && overlay.last_move->first == r && overlay.last_move->second == c;
                if (highlighted) outline = theme.last_move;

                // Once the match ends the winning chain stays solid and everything
                // else fades towards the background.
                if (dimming && !listed(overlay.winning_path, r, c)) {
                    fill = blend(fill, theme.background, DIM_AMOUNT);
                    outline = blend(outline, theme.background, DIM_AMOUNT);
                }

                // While appearing, the stone is also more transparent.
                if (appearing) fill.a = static_cast<std::uint8_t>(255.0f * grow);

                cell.setFillColor(fill);
                cell.setOutlineThickness(highlighted ? thickness * 2.0f : thickness);
                cell.setOutlineColor(outline);

                target.draw(cell);

                // The accessibility mark follows the cell at the same scale and
                // opacity; otherwise it would snap into place over a stone still
                // growing.
                const hex::Piece piece = cells[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
                // The colourblind mark distinguishes the two stones. A walled cell
                // belongs to neither, and marking it would read as a third faction.
                // Its inertness is already clear from the colour, the one case where
                // hue alone suffices.
                if (theme.colorblind
                    && (piece == hex::Piece::RED_DISC || piece == hex::Piece::BLUE_DISC)) {
                    float opacity = appearing ? grow : 1.0f;
                    if (dimming && !listed(overlay.winning_path, r, c)) opacity *= 1.0f - DIM_AMOUNT;
                    drawSymbol(target, r, c, piece, grow, opacity);
                }
            }
        }

        drawBorders(target, n, dimming);

        // Above the stones and below the labels: the glow should light the chain,
        // not obscure the row and column references.
        if (dimming) drawWinSweep(target, overlay.winning_path, overlay.win_elapsed);

        // The tutorial hint: the same glow with a stronger pulse. Whoever is looking
        // does not yet know where to look and has to find it at once.
        if (overlay.hint) {
            const float lit = hexanim::hintPulse(overlay.hint_elapsed);

            sweep.clear();
            appendGlow(layout.centreOf(overlay.hint->first, overlay.hint->second),
                       layout.circumradius() * (0.9f + 0.45f * lit), theme.last_move, lit * 0.9f);
            target.draw(sweep, sf::RenderStates{sf::BlendAdd});
        }

        drawLabels(target, n);
    }

    void SfmlBoardRenderer::drawSymbol(sf::RenderTarget& target, const int row, const int col,
                                       const hex::Piece piece, const float scale,
                                       const float opacity) {
        const Point centre = layout.centreOf(row, col);
        const float r = layout.circumradius() * 0.42f * scale;

        // Two shapes that stay distinct when small and in greyscale alike: the
        // triangle has one vertex on top, the diamond has two at its sides.
        if (piece == hex::Piece::RED_DISC) {
            symbol.setPointCount(3);
            symbol.setPoint(0, {0.0f, -r});
            symbol.setPoint(1, {r * 0.87f, r * 0.5f});
            symbol.setPoint(2, {-r * 0.87f, r * 0.5f});
        } else {
            symbol.setPointCount(4);
            symbol.setPoint(0, {0.0f, -r});
            symbol.setPoint(1, {r, 0.0f});
            symbol.setPoint(2, {0.0f, r});
            symbol.setPoint(3, {-r, 0.0f});
        }

        symbol.setPosition(toSfml(centre));
        symbol.setFillColor(fade(theme.symbol, opacity));
        target.draw(symbol);
    }

    void SfmlBoardRenderer::drawWinSweep(sf::RenderTarget& target,
                                         const std::span<const std::pair<int, int>> path,
                                         const float elapsed) {
        const auto count = static_cast<int>(path.size());
        if (count == 0) return;

        const float head = hexanim::sweepHead(elapsed, count);
        const float radius = layout.circumradius();

        sweep.clear();

        for (int i = 0; i < count; ++i) {
            const float lit = std::max(hexanim::sweepIntensity(i, head), hexanim::WIN_SWEEP_FLOOR);
            const auto [r, c] = path[static_cast<std::size_t>(i)];

            appendGlow(layout.centreOf(r, c), radius * (0.62f + 0.5f * lit),
                       theme.win_glow, lit * 0.85f);
        }

        // Additive blending: two overlapping glows sum instead of replacing each
        // other, so the chain reads as one continuous line of light rather than a row
        // of separate discs.
        target.draw(sweep, sf::RenderStates{sf::BlendAdd});
    }

    void SfmlBoardRenderer::appendGlow(const Point centre, const float radius,
                                       const sf::Color colour, const float intensity) {
        constexpr int FAN_SIDES = 6;
        constexpr float TWO_PI = 6.2831853f;

        const sf::Color hot = fade(colour, intensity);
        const sf::Color cold = fade(colour, 0.0f);
        const sf::Vector2f o{centre.x, centre.y};

        for (int k = 0; k < FAN_SIDES; ++k) {
            const float a0 = TWO_PI * static_cast<float>(k) / static_cast<float>(FAN_SIDES);
            const float a1 = TWO_PI * static_cast<float>(k + 1) / static_cast<float>(FAN_SIDES);

            const sf::Vector2f p0{centre.x + radius * std::cos(a0),
                                  centre.y + radius * std::sin(a0)};
            const sf::Vector2f p1{centre.x + radius * std::cos(a1),
                                  centre.y + radius * std::sin(a1)};

            sweep.append(sf::Vertex{o, hot});
            sweep.append(sf::Vertex{p0, cold});
            sweep.append(sf::Vertex{p1, cold});
        }
    }

    void SfmlBoardRenderer::drawEdge(sf::RenderTarget& target, const int row, const int col,
                                     const int edge, const sf::Color color) {
        const std::array<Point, 6> v = layout.verticesOf(row, col);
        const Point a = v[static_cast<std::size_t>(edge)];
        const Point b = v[static_cast<std::size_t>((edge + 1) % 6)];

        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        const float thickness = std::max(2.0f, layout.circumradius() * theme.border_thickness);

        sf::RectangleShape bar({len, thickness});
        bar.setOrigin({0.0f, thickness / 2.0f});   // centra la barra sul lato
        bar.setPosition(toSfml(a));
        bar.setRotation(sf::radians(std::atan2(dy, dx)));
        bar.setFillColor(color);
        target.draw(bar);
    }

    void SfmlBoardRenderer::drawBorders(sf::RenderTarget& target, const int board_size,
                                        const bool dimmed) {
        const int n = board_size;

        const sf::Color red_edge = dimmed ? blend(theme.edge_red, theme.background, DIM_AMOUNT)
                                          : theme.edge_red;
        const sf::Color blue_edge = dimmed ? blend(theme.edge_blue, theme.background, DIM_AMOUNT)
                                           : theme.edge_blue;

        // Blue first, so Red wins the two shared corners, as in the traditional
        // diagrams where a corner belongs to both sides.
        for (int r = 0; r < n; ++r) {
            drawEdge(target, r, 0, EDGE_LEFT, blue_edge);
            drawEdge(target, r, 0, EDGE_DOWN_LEFT, blue_edge);
            drawEdge(target, r, n - 1, EDGE_RIGHT, blue_edge);
            drawEdge(target, r, n - 1, EDGE_UP_RIGHT, blue_edge);
        }
        for (int c = 0; c < n; ++c) {
            drawEdge(target, 0, c, EDGE_UP_LEFT, red_edge);
            drawEdge(target, 0, c, EDGE_UP_RIGHT, red_edge);
            drawEdge(target, n - 1, c, EDGE_DOWN_LEFT, red_edge);
            drawEdge(target, n - 1, c, EDGE_DOWN_RIGHT, red_edge);
        }
    }

    void SfmlBoardRenderer::drawGhost(sf::RenderTarget& target, const std::pair<int, int> cell_pos,
                                      const hex::Piece piece,
                                      const float elapsed) {   // scala sempre piena
        if (piece == hex::Piece::EMPTY) return;

        // The pulse says "you can play here" without introducing another colour: it
        // is motion, and motion registers where hue does not.
        const float breath = hexanim::ghostPulse(elapsed);

        sf::Color fill = (piece == hex::Piece::RED_DISC) ? theme.piece_red : theme.piece_blue;
        fill.a = static_cast<std::uint8_t>(static_cast<float>(theme.ghost_alpha) * breath);

        shapeCell(cell_pos.first, cell_pos.second);
        cell.setFillColor(fill);
        cell.setOutlineThickness(std::max(1.0f, layout.circumradius() * theme.outline_thickness * 2.0f));

        // Solid outline: the preview stays legible even at the bottom of the pulse,
        // where the fill alone would nearly vanish.
        cell.setOutlineColor(fade(sf::Color(fill.r, fill.g, fill.b), breath));
        target.draw(cell);

        // The preview carries the mark too: without it, in colourblind mode it would
        // be the only stone on the board not stating its colour.
        if (theme.colorblind) {
            drawSymbol(target, cell_pos.first, cell_pos.second, piece, 1.0f, breath * 0.75f);
        }
    }

    void SfmlBoardRenderer::drawHud(sf::RenderTarget& target, const std::vector<std::string>& lines,
                                    const Point top_left, const unsigned char_size) {
        if (!font) return;

        const float line_height = static_cast<float>(char_size) * 1.45f;

        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].empty()) continue;

            sf::Text text(*font, lines[i], char_size);
            // The first line is the primary status: lighter than the rest.
            text.setFillColor(i == 0 ? theme.hud_primary : theme.hud_secondary);
            text.setPosition({top_left.x, top_left.y + line_height * static_cast<float>(i)});
            target.draw(text);
        }
    }

    void SfmlBoardRenderer::drawLabels(sf::RenderTarget& target, const int board_size) {
        if (!font) return;

        const auto size = static_cast<unsigned>(std::max(10.0f, layout.circumradius() * 0.7f));
        const float offset = layout.circumradius() * 1.4f;

        for (int i = 0; i < board_size; ++i) {
            // Column letter, above the first row.
            sf::Text col_label(*font, std::string(1, static_cast<char>('A' + i)), size);
            col_label.setFillColor(theme.label);
            const Point above = layout.centreOf(0, i);
            const sf::FloatRect col_bounds = col_label.getLocalBounds();
            col_label.setOrigin({col_bounds.position.x + col_bounds.size.x / 2.0f,
                                 col_bounds.position.y + col_bounds.size.y / 2.0f});
            col_label.setPosition({above.x, above.y - offset});
            target.draw(col_label);

            // Row number, to the left of the first column.
            sf::Text row_label(*font, std::to_string(i + 1), size);
            row_label.setFillColor(theme.label);
            const Point left = layout.centreOf(i, 0);
            const sf::FloatRect row_bounds = row_label.getLocalBounds();
            row_label.setOrigin({row_bounds.position.x + row_bounds.size.x / 2.0f,
                                 row_bounds.position.y + row_bounds.size.y / 2.0f});
            row_label.setPosition({left.x - offset, left.y});
            target.draw(row_label);
        }
    }
}
