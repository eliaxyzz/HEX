/**
 * @file hex_geometry.h
 * @brief Hexagonal grid geometry: mapping between board indices and pixels.
 */

#ifndef HEX_GEOMETRY_H
#define HEX_GEOMETRY_H

#include <array>
#include <optional>
#include <utility>

namespace hexgui {

    /** @brief Point in pixel space. */
    struct Point {
        float x = 0.0f;
        float y = 0.0f;
    };

    /**
     * @brief Pixel layout of a pointy-top hexagonal board.
     *
     * With S the circumradius (centre to vertex) and origin the centre of cell
     * (0, 0):
     *
     *     centre(r, c) = origin + ( S * sqrt(3) * (c + r/2),  S * 1.5 * r )
     *     vertex_i     = centre + S * ( cos(30 + 60*i), sin(30 + 60*i) )
     *
     * Horizontal pitch is sqrt(3)*S and vertical pitch 1.5*S; the r/2 shear is
     * what gives the board its rhombus shape.
     */
    class HexLayout {
    public:
        /**
         * @brief Builds an explicit layout.
         * @param board_size Cells per side.
         * @param circumradius Centre-to-vertex distance of a cell, in pixels.
         * @param origin Pixel centre of cell (0, 0).
         */
        HexLayout(int board_size, float circumradius, Point origin);

        /**
         * @brief Builds the largest layout fitting the given area, centred in it.
         * @param board_size Cells per side.
         * @param width Available width in pixels.
         * @param height Available height in pixels.
         * @param margin Margin to leave on each side, in pixels.
         */
        [[nodiscard]] static HexLayout fit(int board_size, float width, float height, float margin = 20.0f);

        /** @brief Returns the cells per side. */
        [[nodiscard]] int boardSize() const { return size; }

        /** @brief Returns the circumradius, i.e. the centre-to-vertex distance. */
        [[nodiscard]] float circumradius() const { return S; }

        /** @brief Returns the inradius, i.e. the centre-to-edge distance, S*sqrt(3)/2. */
        [[nodiscard]] float inradius() const;

        /** @brief Returns the distance between adjacent cell centres, sqrt(3)*S. */
        [[nodiscard]] float centreSpacing() const;

        /**
         * @brief Returns the pixel centre of the given cell.
         * @note Out-of-board indices are accepted: the mapping is defined everywhere.
         */
        [[nodiscard]] Point centreOf(int row, int col) const;

        /** @brief Returns the six cell vertices, counter-clockwise from 30 degrees. */
        [[nodiscard]] std::array<Point, 6> verticesOf(int row, int col) const;

        /**
         * @brief Tests whether a point falls inside the given cell's hexagon.
         *
         * Exact test over the three edge normals (0, 60 and 120 degrees): a point is
         * inside when all three projections stay within the inradius. Points exactly
         * on the border count as inside.
         */
        [[nodiscard]] bool contains(int row, int col, Point p) const;

        /**
         * @brief Maps a pixel point to the cell containing it.
         *
         * A hexagonal grid is the Voronoi diagram of its centres, so the containing
         * cell is by construction the one with the nearest centre. The O(n^2) scan is
         * exact and, at click frequency, free: no cube-coordinate rounding and no
         * vertex edge cases.
         *
         * @return The cell, or nullopt if the point falls outside the board.
         */
        [[nodiscard]] std::optional<std::pair<int, int>> cellAt(Point p) const;

        /** @brief Returns the top-left corner of the board bounding box. */
        [[nodiscard]] Point boundsMin() const;

        /** @brief Returns the bottom-right corner of the board bounding box. */
        [[nodiscard]] Point boundsMax() const;

    private:
        /** @brief Cells per side. */
        int size;

        /** @brief Circumradius in pixels. */
        float S;

        /** @brief Pixel centre of cell (0, 0). */
        Point origin;
    };
}

#endif //HEX_GEOMETRY_H
