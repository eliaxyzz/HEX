/**
 * @file hex_geometry.cpp
 * @brief Hexagonal grid geometry implementation.
 */

#include "core/hex_geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace hexgui {

    namespace {
        /** @brief sqrt(3), pervasive in pointy-top geometry. */
        const float SQRT3 = std::sqrt(3.0f);

        /**
         * @brief Board extent expressed in multiples of S.
         *
         *     width  = sqrt(3) * S * (1.5 * (n-1) + 1)
         *     height = S * (1.5 * (n-1) + 2)
         *
         * The first term spans the centres, the second the half cell overhanging
         * each border.
         */
        float widthInS(const int n) { return SQRT3 * (1.5f * static_cast<float>(n - 1) + 1.0f); }
        float heightInS(const int n) { return 1.5f * static_cast<float>(n - 1) + 2.0f; }
    }

    HexLayout::HexLayout(const int board_size, const float circumradius, const Point origin)
        : size(board_size), S(circumradius), origin(origin) {}

    HexLayout HexLayout::fit(const int board_size, const float width, const float height, const float margin) {
        const float usable_w = std::max(1.0f, width - 2.0f * margin);
        const float usable_h = std::max(1.0f, height - 2.0f * margin);

        // The tighter axis dictates the scale.
        const float s = std::min(usable_w / widthInS(board_size),
                                 usable_h / heightInS(board_size));

        // Resulting board extent at that scale.
        const float board_w = s * widthInS(board_size);
        const float board_h = s * heightInS(board_size);

        // Cell (0, 0) sits half a cell in from the bounding box corner.
        const Point origin{
            (width - board_w) / 2.0f + s * SQRT3 / 2.0f,
            (height - board_h) / 2.0f + s
        };

        return {board_size, s, origin};
    }

    float HexLayout::inradius() const { return S * SQRT3 / 2.0f; }

    float HexLayout::centreSpacing() const { return S * SQRT3; }

    Point HexLayout::centreOf(const int row, const int col) const {
        const float r = static_cast<float>(row);
        const float c = static_cast<float>(col);
        return {
            origin.x + S * SQRT3 * (c + r / 2.0f),
            origin.y + S * 1.5f * r
        };
    }

    std::array<Point, 6> HexLayout::verticesOf(const int row, const int col) const {
        const Point centre = centreOf(row, col);
        std::array<Point, 6> pts{};

        for (int i = 0; i < 6; ++i) {
            // Pointy-top vertices: 30, 90, 150, 210, 270 and 330 degrees.
            const float angle = (30.0f + 60.0f * static_cast<float>(i))
                                * std::numbers::pi_v<float> / 180.0f;
            pts[static_cast<std::size_t>(i)] = {
                centre.x + S * std::cos(angle),
                centre.y + S * std::sin(angle)
            };
        }
        return pts;
    }

    bool HexLayout::contains(const int row, const int col, const Point p) const {
        const Point c = centreOf(row, col);
        const float dx = p.x - c.x;
        const float dy = p.y - c.y;

        // Tolerance so that a point exactly on the border belongs to the cell.
        const float limit = inradius() * (1.0f + 1e-4f);

        // Three edge normals at 0, 60 and 120 degrees; the opposite three are
        // covered by taking the absolute value of the projection.
        for (int k = 0; k < 3; ++k) {
            const float angle = 60.0f * static_cast<float>(k) * std::numbers::pi_v<float> / 180.0f;
            const float projection = dx * std::cos(angle) + dy * std::sin(angle);
            if (std::abs(projection) > limit) return false;
        }
        return true;
    }

    std::optional<std::pair<int, int>> HexLayout::cellAt(const Point p) const {
        int best_row = 0;
        int best_col = 0;
        float best_d2 = std::numeric_limits<float>::max();

        for (int r = 0; r < size; ++r) {
            for (int c = 0; c < size; ++c) {
                const Point centre = centreOf(r, c);
                const float dx = p.x - centre.x;
                const float dy = p.y - centre.y;
                if (const float d2 = dx * dx + dy * dy; d2 < best_d2) {
                    best_d2 = d2;
                    best_row = r;
                    best_col = c;
                }
            }
        }

        // The nearest centre identifies the right cell only if the point really
        // lies inside its hexagon; otherwise the click missed the board.
        if (!contains(best_row, best_col, p)) return std::nullopt;
        return std::make_pair(best_row, best_col);
    }

    Point HexLayout::boundsMin() const {
        // Leftmost cell is (n-1, 0), topmost is row 0.
        return {centreOf(0, 0).x - S * SQRT3 / 2.0f, centreOf(0, 0).y - S};
    }

    Point HexLayout::boundsMax() const {
        // Rightmost cell is (n-1, n-1), bottommost is row n-1.
        const Point far = centreOf(size - 1, size - 1);
        return {far.x + S * SQRT3 / 2.0f, far.y + S};
    }
}
