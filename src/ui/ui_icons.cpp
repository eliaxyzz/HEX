/**
 * @file ui_icons.cpp
 * @brief Geometric icon implementation.
 */

#include "ui/ui_icons.h"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>

namespace hexgui {

    namespace {
        /** @brief Filled rectangle, centred on the given point. */
        void box(sf::RenderTarget& t, const Point centre, const float w, const float h,
                 const sf::Color c, const float rotation = 0.0f) {
            sf::RectangleShape r({w, h});
            r.setOrigin({w / 2.0f, h / 2.0f});
            r.setPosition({centre.x, centre.y});
            r.setRotation(sf::degrees(rotation));
            r.setFillColor(c);
            t.draw(r);
        }

        /** @brief Rectangular outline, without fill. */
        void frame(sf::RenderTarget& t, const Point centre, const float w, const float h,
                   const float thickness, const sf::Color c) {
            sf::RectangleShape r({w, h});
            r.setOrigin({w / 2.0f, h / 2.0f});
            r.setPosition({centre.x, centre.y});
            r.setFillColor(sf::Color::Transparent);
            r.setOutlineThickness(thickness);
            r.setOutlineColor(c);
            t.draw(r);
        }

        /**
         * @brief Thick segment between two points, with rounded ends.
         * @note bar() draws around a centre at an angle, convenient for orthogonal
         * strokes and awkward for a polyline, where the endpoints are known and the
         * angle is not. The two discs at the ends act as joints; without them a notch
         * would show between one segment and the next.
         */
        void segment(sf::RenderTarget& t, const Point a, const Point b,
                     const float thickness, const sf::Color c) {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.0f) return;

            sf::RectangleShape r({len, thickness});
            r.setOrigin({0.0f, thickness / 2.0f});
            r.setPosition({a.x, a.y});
            r.setRotation(sf::radians(std::atan2(dy, dx)));
            r.setFillColor(c);
            t.draw(r);

            sf::CircleShape cap(thickness / 2.0f);
            cap.setOrigin({thickness / 2.0f, thickness / 2.0f});
            cap.setFillColor(c);

            cap.setPosition({a.x, a.y});
            t.draw(cap);
            cap.setPosition({b.x, b.y});
            t.draw(cap);
        }

        /** @brief Filled disc. */
        void disc(sf::RenderTarget& t, const Point centre, const float radius, const sf::Color c) {
            sf::CircleShape s(radius);
            s.setOrigin({radius, radius});
            s.setPosition({centre.x, centre.y});
            s.setFillColor(c);
            t.draw(s);
        }

        /** @brief Ring: a circle with outline only. */
        void ring(sf::RenderTarget& t, const Point centre, const float radius,
                  const float thickness, const sf::Color c) {
            sf::CircleShape s(radius);
            s.setOrigin({radius, radius});
            s.setPosition({centre.x, centre.y});
            s.setFillColor(sf::Color::Transparent);
            s.setOutlineThickness(thickness);
            s.setOutlineColor(c);
            t.draw(s);
        }

        /** @brief Segment between two points, with a thickness. */
        void line(sf::RenderTarget& t, const Point a, const Point b,
                  const float thickness, const sf::Color c) {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.0f) return;

            sf::RectangleShape r({len, thickness});
            r.setOrigin({0.0f, thickness / 2.0f});
            r.setPosition({a.x, a.y});
            r.setRotation(sf::radians(std::atan2(dy, dx)));
            r.setFillColor(c);
            t.draw(r);
        }

        /** @brief Equilateral triangle pointing right. */
        void triangle(sf::RenderTarget& t, const Point centre, const float size, const sf::Color c) {
            sf::CircleShape s(size / 2.0f, 3);
            s.setOrigin({size / 2.0f, size / 2.0f});
            s.setPosition({centre.x, centre.y});
            s.setRotation(sf::degrees(90.0f));
            s.setFillColor(c);
            t.draw(s);
        }
    }

    float iconSlot(const float size) {
        return size + size * 0.55f;
    }

    void drawIcon(sf::RenderTarget& target, const Icon icon, const Point centre,
                  const float size, const sf::Color colour) {
        if (icon == Icon::NONE || colour.a == 0) return;

        // Everything is expressed in fractions of the side, so changing `size`
        // rescales the whole icon without touching a single constant.
        const float u = size / 2.0f;                       // mezzo lato
        const float stroke = std::max(1.2f, size * 0.11f); // spessore del tratto

        switch (icon) {
            case Icon::AI: {
                // A boxy face with an antenna: the machine that plays.
                frame(target, {centre.x, centre.y + u * 0.15f}, size * 0.82f, size * 0.68f,
                      -stroke, colour);
                disc(target, {centre.x - u * 0.24f, centre.y + u * 0.05f}, stroke * 0.9f, colour);
                disc(target, {centre.x + u * 0.24f, centre.y + u * 0.05f}, stroke * 0.9f, colour);
                line(target, {centre.x, centre.y - u * 0.5f}, {centre.x, centre.y - u * 0.86f},
                     stroke * 0.8f, colour);
                disc(target, {centre.x, centre.y - u * 0.9f}, stroke * 0.85f, colour);
                break;
            }

            case Icon::PEOPLE: {
                // Two heads side by side. At this size shoulders blur into a smudge:
                // two circles read, five shapes do not.
                disc(target, {centre.x - u * 0.42f, centre.y - u * 0.1f}, size * 0.26f, colour);
                disc(target, {centre.x + u * 0.42f, centre.y - u * 0.1f}, size * 0.26f, colour);
                box(target, {centre.x, centre.y + u * 0.62f}, size * 0.9f, size * 0.22f, colour);
                break;
            }

            case Icon::ONLINE: {
                // A screen on its stand: the network match.
                frame(target, {centre.x, centre.y - u * 0.18f}, size * 0.88f, size * 0.62f,
                      -stroke, colour);
                line(target, {centre.x - u * 0.36f, centre.y + u * 0.72f},
                     {centre.x + u * 0.36f, centre.y + u * 0.72f}, stroke, colour);
                line(target, {centre.x, centre.y + u * 0.13f}, {centre.x, centre.y + u * 0.72f},
                     stroke * 0.8f, colour);
                break;
            }

            case Icon::SETTINGS: {
                // A ring with teeth around it: the gear.
                ring(target, centre, size * 0.27f, stroke, colour);
                for (int i = 0; i < 6; ++i) {
                    const float angle = static_cast<float>(i) * std::numbers::pi_v<float> / 3.0f;
                    const float r0 = size * 0.36f;
                    box(target, {centre.x + std::cos(angle) * r0, centre.y + std::sin(angle) * r0},
                        size * 0.17f, stroke * 1.1f, colour,
                        angle * 180.0f / std::numbers::pi_v<float>);
                }
                break;
            }

            case Icon::FOLDER: {
                // The tab and the body of a folder.
                line(target, {centre.x - u * 0.72f, centre.y - u * 0.42f},
                     {centre.x - u * 0.12f, centre.y - u * 0.42f}, stroke, colour);
                frame(target, {centre.x, centre.y + u * 0.16f}, size * 0.86f, size * 0.56f,
                      -stroke, colour);
                break;
            }

            case Icon::EXIT: {
                // A half-open door with its handle.
                frame(target, {centre.x - u * 0.15f, centre.y}, size * 0.6f, size * 0.9f,
                      -stroke, colour);
                disc(target, {centre.x + u * 0.02f, centre.y + u * 0.08f}, stroke * 0.8f, colour);
                line(target, {centre.x + u * 0.42f, centre.y}, {centre.x + u * 0.86f, centre.y},
                     stroke * 0.8f, colour);
                break;
            }

            case Icon::LEVEL_1:
            case Icon::LEVEL_2:
            case Icon::LEVEL_3: {
                // Rising bars, like a signal strength meter: how many there are says
                // the level, and they are counted at a glance. Crossed blades would
                // read as an X at this size.
                const int filled = 1 + static_cast<int>(icon) - static_cast<int>(Icon::LEVEL_1);
                const float bar_w = size * 0.2f;
                const float step = size * 0.3f;

                for (int i = 0; i < 3; ++i) {
                    const float h = size * (0.3f + 0.24f * static_cast<float>(i));
                    const float x = centre.x + (static_cast<float>(i) - 1.0f) * step;
                    const float y = centre.y + u * 0.7f - h / 2.0f;

                    sf::Color c = colour;
                    // Bars beyond the level remain, but barely sketched: the whole
                    // scale is visible along with one's position in it.
                    if (i >= filled) c.a = static_cast<std::uint8_t>(c.a * 0.28f);

                    box(target, {x, y}, bar_w, h, c);
                }
                break;
            }

            case Icon::PLAY:
                triangle(target, {centre.x + u * 0.08f, centre.y}, size * 0.86f, colour);
                break;

            case Icon::USER:
                disc(target, {centre.x, centre.y - u * 0.32f}, size * 0.22f, colour);
                box(target, {centre.x, centre.y + u * 0.45f}, size * 0.62f, size * 0.34f, colour);
                break;

            case Icon::DISC_RED:
            case Icon::DISC_BLUE:
                // The stone itself. The caller supplies the colour, so the icon stays
                // consistent with the theme instead of carrying one of its own.
                disc(target, centre, size * 0.36f, colour);
                break;

            case Icon::HELP: {
                // A question mark built from the same strokes as the other icons
                // rather than set in the font: a text glyph inside a geometric
                // repertoire stands out as an intruder.
                const float r = size * 0.28f;
                const float w = size * 0.12f;

                // The upper arc approximated by three segments: enough to read as a
                // hook, and no curve primitive introduced for a single case.
                segment(target, {centre.x - r, centre.y - r * 0.55f},
                                {centre.x - r * 0.5f, centre.y - r * 1.25f}, w, colour);
                segment(target, {centre.x - r * 0.5f, centre.y - r * 1.25f},
                                {centre.x + r * 0.5f, centre.y - r * 1.25f}, w, colour);
                segment(target, {centre.x + r * 0.5f, centre.y - r * 1.25f},
                                {centre.x + r * 0.85f, centre.y - r * 0.35f}, w, colour);

                // The descent towards the centre and the stem that follows it.
                segment(target, {centre.x + r * 0.85f, centre.y - r * 0.35f},
                                {centre.x, centre.y + r * 0.25f}, w, colour);
                segment(target, {centre.x, centre.y + r * 0.25f},
                                {centre.x, centre.y + r * 0.7f}, w, colour);

                // The dot, detached: it is what makes the mark a question.
                disc(target, {centre.x, centre.y + r * 1.3f}, w * 0.62f, colour);
                break;
            }

            case Icon::TRASH: {
                // Lid, handle and body. The three vertical grooves are what separates
                // a waste bin from a generic container: without them, at this size, it
                // stays a rectangle with a line on top.
                const float lid_y = centre.y - u * 0.52f;

                line(target, {centre.x - u * 0.62f, lid_y}, {centre.x + u * 0.62f, lid_y},
                     stroke, colour);
                box(target, {centre.x, lid_y - stroke * 1.1f}, size * 0.3f, stroke * 0.9f, colour);

                frame(target, {centre.x, centre.y + u * 0.28f}, size * 0.82f, size * 0.7f,
                      -stroke * 0.85f, colour);

                for (int i = -1; i <= 1; ++i) {
                    const float x = centre.x + static_cast<float>(i) * u * 0.26f;
                    line(target, {x, centre.y - u * 0.02f}, {x, centre.y + u * 0.5f},
                         stroke * 0.6f, colour);
                }
                break;
            }

            case Icon::PENCIL: {
                // A diagonal from bottom left to top right: the pencil's body. The
                // tip is a disc at the lower end and the ferrule a thicker stroke just
                // below the top. Two details and no more, because at 16 pixels a third
                // turns to dirt.
                const Point tip{centre.x - u * 0.68f, centre.y + u * 0.68f};
                const Point top{centre.x + u * 0.68f, centre.y - u * 0.68f};

                // The body is thick: a thin diagonal lacks the mass that makes it read
                // as an object rather than as a scratch.
                segment(target, tip, top, stroke * 2.2f, colour);

                // The tip narrows towards the end and the ferrule interrupts it a
                // third of the way down. These are the two points at which a pencil
                // stops being a tilted stick.
                disc(target, tip, stroke * 0.7f, colour);
                segment(target, {centre.x + u * 0.14f, centre.y - u * 0.14f},
                                {centre.x + u * 0.36f, centre.y - u * 0.36f},
                        stroke * 3.2f, colour);
                break;
            }

            case Icon::BLACK_HOLE: {
                // A ring and nothing inside it: the same figure a walled cell shows on
                // the board, so the menu switch and its consequence recognise each
                // other.
                ring(target, centre, size * 0.34f, stroke, colour);
                break;
            }

            default:
                break;
        }
    }
}
