/**
 * @file sfml_widgets.cpp
 * @brief Widget drawing implementation.
 */

#include "ui/sfml_widgets.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <numbers>
#include <sstream>
#include <utility>
#include <vector>

namespace hexgui {

    namespace {
        /** @brief Returns the texture identifier matching a button state. */
        const char* buttonTexture(const hexui::WidgetState state) {
            switch (state) {
                case hexui::WidgetState::HOVERED:  return hexassets::textures::BUTTON_HOVER;
                case hexui::WidgetState::PRESSED:  return hexassets::textures::BUTTON_PRESSED;
                case hexui::WidgetState::DISABLED: return hexassets::textures::BUTTON_DISABLED;
                default:                           return hexassets::textures::BUTTON_NORMAL;
            }
        }

        /** @brief Returns the texture identifier matching a text field state. */
        const char* inputTexture(const hexui::WidgetState state) {
            switch (state) {
                case hexui::WidgetState::PRESSED: return hexassets::textures::INPUT_FOCUS;
                case hexui::WidgetState::HOVERED: return hexassets::textures::INPUT_HOVER;
                default:                          return hexassets::textures::INPUT_NORMAL;
            }
        }
    }

    sf::Color withOpacity(sf::Color color, const float opacity) {
        color.a = static_cast<std::uint8_t>(static_cast<float>(color.a)
                                            * std::clamp(opacity, 0.0f, 1.0f));
        return color;
    }

    void drawNineSlice(sf::RenderTarget& target, const sf::Texture& texture,
                       const hexui::Rect& dest, const int border, const sf::Color tint) {
        const sf::Vector2u size = texture.getSize();
        if (size.x == 0 || size.y == 0 || dest.w <= 0.0f || dest.h <= 0.0f) return;

        const auto tw = static_cast<int>(size.x);
        const auto th = static_cast<int>(size.y);

        // The border cannot exceed half the texture nor half the destination, or the
        // slices would overlap and the centre would take a negative width.
        const int b = std::max(0, std::min({border, tw / 2, th / 2,
                                            static_cast<int>(dest.w / 2.0f),
                                            static_cast<int>(dest.h / 2.0f)}));
        const auto bf = static_cast<float>(b);

        // Columns and rows: [corner | stretchable span | corner], in texture
        // coordinates and in destination coordinates.
        const int src_x[4]{0, b, tw - b, tw};
        const int src_y[4]{0, b, th - b, th};
        const float dst_x[4]{dest.x, dest.x + bf, dest.x + dest.w - bf, dest.x + dest.w};
        const float dst_y[4]{dest.y, dest.y + bf, dest.y + dest.h - bf, dest.y + dest.h};

        sf::Sprite piece(texture);
        piece.setColor(tint);

        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                const int sw = src_x[col + 1] - src_x[col];
                const int sh = src_y[row + 1] - src_y[row];
                const float dw = dst_x[col + 1] - dst_x[col];
                const float dh = dst_y[row + 1] - dst_y[row];
                if (sw <= 0 || sh <= 0 || dw <= 0.0f || dh <= 0.0f) continue;

                piece.setTextureRect(sf::IntRect({src_x[col], src_y[row]}, {sw, sh}));
                piece.setPosition({dst_x[col], dst_y[row]});
                piece.setScale({dw / static_cast<float>(sw), dh / static_cast<float>(sh)});

                target.draw(piece);
            }
        }
    }

    void drawCenteredText(sf::RenderTarget& target, const std::string& text,
                          const hexui::Rect& area, const sf::Font* font,
                          const unsigned char_size, const sf::Color color) {
        if (!font || text.empty() || color.a == 0) return;

        sf::Text label(*font, text, char_size);
        label.setFillColor(color);

        const sf::FloatRect b = label.getLocalBounds();
        label.setOrigin({b.position.x + b.size.x / 2.0f, b.position.y + b.size.y / 2.0f});
        label.setPosition({area.x + area.w / 2.0f, area.y + area.h / 2.0f});

        target.draw(label);
    }

    std::size_t drawWrappedText(sf::RenderTarget& target, const std::string& text,
                                const hexui::Rect& area, const sf::Font* font,
                                const unsigned char_size, const sf::Color color) {
        if (!font || text.empty() || color.a == 0) return 0;

        // Measured with the very object that will draw it: any other width estimate
        // would be a second source of truth, and it would eventually diverge.
        sf::Text probe(*font, "", char_size);

        const auto widthOf = [&probe](const std::string& s) {
            probe.setString(s);
            return probe.getLocalBounds().size.x;
        };

        std::vector<std::string> lines;
        std::string current;

        std::istringstream words(text);
        std::string word;

        while (words >> word) {
            const std::string candidate = current.empty() ? word : current + ' ' + word;

            if (current.empty() || widthOf(candidate) <= area.w) {
                current = candidate;
                continue;
            }

            lines.push_back(current);
            current = word;
        }
        if (!current.empty()) lines.push_back(current);

        const float line_height = static_cast<float>(char_size) * 1.35f;

        for (std::size_t i = 0; i < lines.size(); ++i) {
            drawCenteredText(target, lines[i],
                             {area.x, area.y + line_height * static_cast<float>(i),
                              area.w, line_height},
                             font, char_size, color);
        }

        return lines.size();
    }

    void drawButton(sf::RenderTarget& target, const hexui::Button& button,
                    hexassets::AssetManager& assets, const Theme& theme, const float opacity) {
        const hexui::WidgetState state = button.state();

        drawNineSlice(target, assets.texture(buttonTexture(state)), button.bounds(),
                      hexassets::PLACEHOLDER_BORDER, withOpacity(sf::Color::White, opacity));

        const sf::Color text_color = state == hexui::WidgetState::DISABLED ? theme.ui_text_muted
                                                                          : theme.ui_text;
        drawCenteredText(target, button.label(), button.bounds(), assets.font(), 20,
                         withOpacity(text_color, opacity));
    }

    void drawTextInput(sf::RenderTarget& target, const hexui::TextInput& input,
                       hexassets::AssetManager& assets, const Theme& theme, const float opacity) {
        const hexui::Rect& r = input.bounds();

        drawNineSlice(target, assets.texture(inputTexture(input.state())), r,
                      hexassets::PLACEHOLDER_BORDER, withOpacity(sf::Color::White, opacity));

        const sf::Font* font = assets.font();
        if (!font) return;

        const bool empty = input.text().empty();
        constexpr unsigned SIZE = 20;
        constexpr float PADDING = 14.0f;

        sf::Text label(*font, empty ? input.placeholder() : input.text(), SIZE);
        label.setFillColor(withOpacity(empty ? theme.ui_text_muted : theme.ui_text, opacity));

        const sf::FloatRect b = label.getLocalBounds();
        label.setOrigin({0.0f, b.position.y + b.size.y / 2.0f});
        label.setPosition({r.x + PADDING, r.y + r.h / 2.0f});
        target.draw(label);

        // Caret: a bar immediately after the last character.
        if (input.caretVisible()) {
            const float text_width = empty ? 0.0f : b.size.x;

            sf::RectangleShape caret({2.0f, static_cast<float>(SIZE)});
            caret.setPosition({r.x + PADDING + text_width + 2.0f, r.y + r.h / 2.0f - SIZE / 2.0f});
            caret.setFillColor(withOpacity(theme.ui_text, opacity));
            target.draw(caret);
        }
    }


    // --- Flat style -----------------------------------------------------------

    namespace {
        /** @brief Builds the outline of a rounded rectangle. */
        sf::ConvexShape roundedShape(const hexui::Rect& box, float radius) {
            // The radius cannot exceed half the short side; past that the two arcs on
            // one side would overlap and the shape would fold in on itself.
            radius = std::clamp(radius, 0.0f, std::min(box.w, box.h) / 2.0f);

            constexpr int PER_CORNER = 8;   // abbastanza per non vedere gli spigoli
            sf::ConvexShape shape;
            shape.setPointCount(PER_CORNER * 4);

            // The four arc centres, clockwise from the top-right corner.
            const std::array<Point, 4> centres{
                Point{box.x + box.w - radius, box.y + radius},
                Point{box.x + box.w - radius, box.y + box.h - radius},
                Point{box.x + radius,         box.y + box.h - radius},
                Point{box.x + radius,         box.y + radius}};

            std::size_t index = 0;
            for (int corner = 0; corner < 4; ++corner) {
                // Each corner spans a quarter turn, starting at -90 degrees.
                const float start = -std::numbers::pi_v<float> / 2.0f
                                  + static_cast<float>(corner) * std::numbers::pi_v<float> / 2.0f;

                for (int i = 0; i < PER_CORNER; ++i) {
                    const float t = static_cast<float>(i) / static_cast<float>(PER_CORNER - 1);
                    const float angle = start + t * std::numbers::pi_v<float> / 2.0f;

                    shape.setPoint(index++, {centres[corner].x + std::cos(angle) * radius,
                                             centres[corner].y + std::sin(angle) * radius});
                }
            }
            return shape;
        }

        /** @brief Returns the text colour for a button's style and state. */
        sf::Color labelColour(const Theme& theme, const ButtonStyle style,
                              const hexui::WidgetState state) {
            if (state == hexui::WidgetState::DISABLED) return theme.ui_text_muted;
            if (style == ButtonStyle::PRIMARY) return theme.hud_primary;
            return state == hexui::WidgetState::NORMAL ? theme.hud_secondary : theme.ui_text;
        }

        /** @brief Draws icon and label in two distinct colours. */
        void drawIconAndLabelSplit(sf::RenderTarget& target, const std::string& label,
                                   const hexui::Rect& area, const sf::Font* font,
                                   unsigned char_size, sf::Color text_colour,
                                   sf::Color icon_colour, Icon icon, float icon_size);

        /** @brief Returns the text width at that size, zero when there is nothing to measure. */
        float labelWidth(const std::string& label, const sf::Font* font, const unsigned char_size) {
            if (!font || label.empty()) return 0.0f;

            const sf::Text probe(*font, label, char_size);
            return probe.getLocalBounds().size.x;
        }

        /** @brief Text and icon size after fitting to the bounds. */
        struct FittedText {
            unsigned char_size = 0;
            float icon_size = 0.0f;
        };

        /** @brief Minimum size; below this threshold the text stops being legible. */
        constexpr unsigned MIN_CHAR_SIZE = 10;

        /**
         * @brief Shrinks icon and label until they fit inside the bounds.
         *
         * The button's style decides the nominal text size, not its width, which is
         * right: a primary action should not look different depending on where it
         * lands. But a size chosen without consulting the bounds is one that
         * eventually does not fit, and an over-wide group is centred and then
         * overflows on both sides.
         *
         * The nominal size therefore remains the starting point and is reduced only
         * when necessary.
         *
         * @note Text width grows almost linearly with size, so one ratio plus two
         * corrections suffice: the right size is reached without trying every size in
         * turn on every frame.
         */
        FittedText fitToBox(const std::string& label, const hexui::Rect& box, const sf::Font* font,
                            const unsigned char_size, const Icon icon, const float icon_size,
                            const float padding) {
            const bool has_icon = icon != Icon::NONE;
            const auto needed = [&](const unsigned size, const float icons) {
                return (has_icon ? iconSlot(icons) : 0.0f) + labelWidth(label, font, size);
            };

            const float available = box.w - 2.0f * padding;
            if (available <= 0.0f) return {char_size, icon_size};

            FittedText fitted{char_size, icon_size};

            // Three passes at most: the first estimates the ratio, the other two
            // correct it when the real measurement misses what the ratio promised.
            for (int attempt = 0; attempt < 3; ++attempt) {
                const float width = needed(fitted.char_size, fitted.icon_size);
                if (width <= available || fitted.char_size <= MIN_CHAR_SIZE) break;

                const float scale = available / width;
                const auto scaled = static_cast<unsigned>(static_cast<float>(fitted.char_size) * scale);

                fitted.char_size = std::max(MIN_CHAR_SIZE, std::min(fitted.char_size - 1, scaled));
                fitted.icon_size = icon_size * static_cast<float>(fitted.char_size)
                                 / static_cast<float>(char_size);
            }

            return fitted;
        }

        /**
         * @brief Draws icon and label as a single centred block.
         * @note Measuring the text before placing it is the only way to keep the group
         * centred: with the icon pinned left and the text centred on its own, the
         * visual centre shifts with every translation.
         */
        void drawIconAndLabel(sf::RenderTarget& target, const std::string& label,
                              const hexui::Rect& area, const sf::Font* font,
                              const unsigned char_size, const sf::Color colour,
                              const Icon icon, const float icon_size) {
            drawIconAndLabelSplit(target, label, area, font, char_size, colour, colour,
                                  icon, icon_size);
        }

        void drawIconAndLabelSplit(sf::RenderTarget& target, const std::string& label,
                                   const hexui::Rect& area, const sf::Font* font,
                                   const unsigned char_size, const sf::Color text_colour,
                                   const sf::Color icon_colour, const Icon icon,
                                   const float icon_size) {
            const bool has_icon = icon != Icon::NONE;
            const float slot = has_icon ? iconSlot(icon_size) : 0.0f;

            float text_width = 0.0f;
            if (font && !label.empty()) {
                const sf::Text probe(*font, label, char_size);
                text_width = probe.getLocalBounds().size.x;
            }

            const float total = slot + text_width;
            const float left = area.x + (area.w - total) / 2.0f;
            const float middle = area.y + area.h / 2.0f;

            if (has_icon) {
                drawIcon(target, icon, {left + icon_size / 2.0f, middle}, icon_size, icon_colour);
            }

            if (font && !label.empty()) {
                sf::Text text(*font, label, char_size);
                text.setFillColor(text_colour);

                const sf::FloatRect b = text.getLocalBounds();
                text.setOrigin({b.position.x, b.position.y + b.size.y / 2.0f});
                text.setPosition({left + slot, middle});
                target.draw(text);
            }
        }
    }

    void drawRoundedRect(sf::RenderTarget& target, const hexui::Rect& box, const float radius,
                         const sf::Color fill, const sf::Color outline,
                         const float outline_thickness) {
        if (box.w <= 0.0f || box.h <= 0.0f) return;

        sf::ConvexShape shape = roundedShape(box, radius);
        shape.setFillColor(fill);

        if (outline_thickness > 0.0f) {
            shape.setOutlineThickness(outline_thickness);
            shape.setOutlineColor(outline);
        }
        target.draw(shape);
    }

    void drawFlatButton(sf::RenderTarget& target, const hexui::Button& button,
                        const sf::Font* font, const Theme& theme, const ButtonStyle style,
                        const Icon icon, const float opacity) {
        const hexui::Rect& box = button.bounds();
        const hexui::WidgetState state = button.state();
        const bool disabled = state == hexui::WidgetState::DISABLED;
        const bool active = state == hexui::WidgetState::HOVERED
                         || state == hexui::WidgetState::PRESSED;

        // Near-semicircular corners: the shape that separates a pill from a
        // rectangle with rounded corners.
        const float radius = box.h * 0.46f;

        sf::Color fill = theme.ui_fill;
        sf::Color border = theme.ui_border;
        float thickness = 1.0f;

        switch (style) {
            case ButtonStyle::PRIMARY:
                // The dominant action: solid surface with a slightly lighter border
                // that lifts it off the ground without framing it.
                fill = disabled ? theme.ui_fill_disabled : theme.accent_deep;
                if (active && !disabled) fill = theme.accent_bright;
                border = disabled ? theme.ui_border_soft : theme.ui_accent;
                thickness = 1.6f;
                break;

            case ButtonStyle::SECONDARY:
                fill = active ? theme.ui_fill_hover : theme.ui_surface;
                border = active ? theme.ui_accent : theme.ui_border;
                thickness = 1.4f;
                break;

            case ButtonStyle::GHOST:
                fill = active ? theme.ui_surface : sf::Color::Transparent;
                border = active ? theme.ui_border : theme.ui_border_soft;
                thickness = 1.2f;
                break;
        }

        if (disabled) {
            fill = theme.ui_fill_disabled;
            border = theme.ui_border_soft;
        }

        // A barely perceptible glow under the primary action: what makes it look lit
        // rather than merely coloured.
        if (style == ButtonStyle::PRIMARY && !disabled) {
            const hexui::Rect halo{box.x - 5.0f, box.y - 5.0f, box.w + 10.0f, box.h + 10.0f};
            drawRoundedRect(target, halo, radius + 5.0f,
                            withOpacity(theme.ui_accent, opacity * 0.16f));
        }

        drawRoundedRect(target, box, radius, withOpacity(fill, opacity),
                        withOpacity(border, opacity), thickness);

        const unsigned size = style == ButtonStyle::PRIMARY ? 26u : 16u;
        const float icon_size = style == ButtonStyle::PRIMARY ? 22.0f : 17.0f;

        // The bounds are a container, not a suggestion: icon and label fit themselves
        // to it rather than spilling out.
        constexpr float LABEL_PADDING = 14.0f;
        const FittedText fitted = fitToBox(button.label(), box, font, size, icon, icon_size,
                                           LABEL_PADDING);

        drawIconAndLabel(target, button.label(), box, font, fitted.char_size,
                         withOpacity(labelColour(theme, style, state), opacity),
                         icon, fitted.icon_size);
    }

    void drawSegmented(sf::RenderTarget& target, const hexui::OptionGroup& group,
                       const sf::Font* font, const Theme& theme,
                       const std::vector<Icon>& icons, const float opacity) {
        const hexui::Rect& box = group.bounds();
        const auto count = static_cast<int>(group.options().size());
        if (count <= 0) return;

        const float radius = box.h / 2.0f;

        // One shape for the whole group: the pill containing the choice.
        drawRoundedRect(target, box, radius, withOpacity(theme.ui_surface, opacity),
                        withOpacity(theme.ui_border_soft, opacity), 1.4f);

        // The selected option: a soft fill, inset slightly so it reads as sliding
        // inside the container rather than coinciding with its border.
        constexpr float INSET = 3.0f;
        const hexui::Rect chosen = group.optionBounds(group.selected());
        const hexui::Rect pill{chosen.x + INSET, chosen.y + INSET,
                               chosen.w - 2.0f * INSET, chosen.h - 2.0f * INSET};

        drawRoundedRect(target, pill, pill.h / 2.0f,
                        withOpacity(theme.ui_fill_active, opacity),
                        withOpacity(theme.ui_accent, opacity * 0.85f), 1.4f);

        for (int i = 0; i < count; ++i) {
            const hexui::Rect cell = group.optionBounds(i);
            const bool is_selected = (i == group.selected());
            const bool is_hovered = (i == group.hovered());

            // A locked option is readable but never lights up: no selection colour,
            // no reaction to the pointer, and a reduced opacity separating it from one
            // that is merely unselected.
            const bool is_locked = !group.optionEnabled(i);

            const sf::Color colour = is_locked   ? theme.ui_text_muted
                                   : is_selected ? theme.hud_primary
                                   : is_hovered  ? theme.ui_text
                                                 : theme.ui_text_muted;

            const float item_opacity = is_locked ? opacity * 0.45f : opacity;

            const Icon icon = (static_cast<std::size_t>(i) < icons.size()) ? icons[i] : Icon::NONE;

            // Stones are drawn in their own colour rather than the text's: choosing
            // to play as Blue must show exactly the blue that will reach the board.
            sf::Color icon_colour = colour;
            if (icon == Icon::DISC_RED)  icon_colour = theme.piece_red;
            if (icon == Icon::DISC_BLUE) icon_colour = theme.piece_blue;
            if (!is_selected && !is_hovered) icon_colour.a = static_cast<std::uint8_t>(
                static_cast<float>(icon_colour.a) * 0.55f);

            drawIconAndLabelSplit(target, group.options()[static_cast<std::size_t>(i)], cell,
                                  font, 16, withOpacity(colour, item_opacity),
                                  withOpacity(icon_colour, item_opacity), icon, 17.0f);
        }
    }

    void drawFlatTextInput(sf::RenderTarget& target, const hexui::TextInput& input,
                           const sf::Font* font, const Theme& theme, const Icon icon,
                           const float opacity) {
        const hexui::Rect& box = input.bounds();
        const bool focused = input.focused();
        const bool hovered = input.state() == hexui::WidgetState::HOVERED;

        drawRoundedRect(target, box, box.h * 0.46f,
                        withOpacity(theme.ui_surface, opacity),
                        withOpacity(focused ? theme.ui_accent
                                            : (hovered ? theme.ui_border : theme.ui_border_soft),
                                    opacity),
                        focused ? 1.8f : 1.4f);

        constexpr unsigned SIZE = 18;
        constexpr float PADDING = 18.0f;

        float left = box.x + PADDING;
        const float middle = box.y + box.h / 2.0f;

        if (icon != Icon::NONE) {
            const float icon_size = 17.0f;
            drawIcon(target, icon, {left + icon_size / 2.0f, middle}, icon_size,
                     withOpacity(theme.ui_text_muted, opacity));
            left += iconSlot(icon_size);
        }

        if (!font) return;

        const bool empty = input.text().empty();
        sf::Text label(*font, empty ? input.placeholder() : input.text(), SIZE);
        label.setFillColor(withOpacity(empty ? theme.ui_text_muted : theme.ui_text, opacity));

        const sf::FloatRect b = label.getLocalBounds();
        label.setOrigin({0.0f, b.position.y + b.size.y / 2.0f});
        label.setPosition({left, middle});
        target.draw(label);

        if (input.caretVisible()) {
            const float text_width = empty ? 0.0f : b.size.x;

            sf::RectangleShape caret({2.0f, static_cast<float>(SIZE)});
            caret.setPosition({left + text_width + 3.0f, middle - SIZE / 2.0f});
            caret.setFillColor(withOpacity(theme.ui_accent, opacity));
            target.draw(caret);
        }
    }

    void drawGlowText(sf::RenderTarget& target, const std::string& text,
                      const hexui::Rect& area, const sf::Font* font, const unsigned char_size,
                      const sf::Color colour, const sf::Color glow, const float radius) {
        if (!font || text.empty()) return;

        // Four diagonal copies, progressively wider and fainter: from a distance they
        // read as a halo, up close they are indistinguishable from the text.
        constexpr int LAYERS = 4;
        for (int i = LAYERS; i >= 1; --i) {
            const float spread = radius * static_cast<float>(i) / LAYERS;
            const float strength = 0.16f * (1.0f - static_cast<float>(i - 1) / LAYERS);

            for (const auto& [dx, dy] : {std::pair{-spread, 0.0f}, std::pair{spread, 0.0f},
                                         std::pair{0.0f, -spread}, std::pair{0.0f, spread}}) {
                const hexui::Rect shifted{area.x + dx, area.y + dy, area.w, area.h};
                drawCenteredText(target, text, shifted, font, char_size,
                                 withOpacity(glow, strength));
            }
        }

        drawCenteredText(target, text, area, font, char_size, colour);
    }

    void drawOptionGroup(sf::RenderTarget& target, const hexui::OptionGroup& group,
                         hexassets::AssetManager& assets, const Theme& theme, const float opacity) {
        for (int i = 0; i < static_cast<int>(group.options().size()); ++i) {
            const hexui::Rect r = group.optionBounds(i);
            const bool is_selected = (i == group.selected());
            const bool is_hovered = (i == group.hovered());

            const char* id = is_selected ? hexassets::textures::OPTION_SELECTED
                           : is_hovered  ? hexassets::textures::OPTION_HOVER
                                         : hexassets::textures::OPTION_NORMAL;

            drawNineSlice(target, assets.texture(id), r, hexassets::PLACEHOLDER_BORDER,
                          withOpacity(sf::Color::White, opacity));

            drawCenteredText(target, group.options()[static_cast<std::size_t>(i)], r,
                             assets.font(), 18,
                             withOpacity(is_selected ? theme.ui_text : theme.ui_text_muted, opacity));
        }
    }
}
