/**
 * @file sfml_theme.h
 * @brief Colour palette of the graphical view.
 *
 * Holding the colours in a struct rather than as constants scattered through the
 * renderer makes any change of appearance a data change instead of a code change,
 * and allows several themes without touching the drawing.
 */

#ifndef SFML_THEME_H
#define SFML_THEME_H

#include <SFML/Graphics/Color.hpp>

#include <cstdint>

#include "core/player_profile.h"

namespace hexgui {

    /**
     * @brief Colours and proportions used to draw the board and the interface.
     */
    struct Theme {
        /** @brief Window background. */
        sf::Color background{18, 20, 26};

        /** @brief Fill of an empty cell. */
        sf::Color cell_empty{38, 43, 54};

        /** @brief Cell outline. */
        sf::Color cell_outline{57, 64, 78};

        /**
         * @brief Fill of a walled cell, the Arcade black hole.
         * @note Darker than the background rather than lighter than an empty cell,
         * which is the only way it reads as "no board here" instead of "a cell of
         * another kind". A livelier tint would look like a third player's stone, that
         * is, like something to be captured.
         */
        sf::Color cell_blocked{12, 13, 17};

        /** @brief Red's stone. */
        sf::Color piece_red{214, 74, 74};

        /** @brief Blue's stone. */
        sf::Color piece_blue{74, 124, 214};

        /** @brief Outline of the last move's cell. */
        sf::Color last_move{240, 196, 90};

        /**
         * @brief Red's edges: the top and bottom sides of the board.
         * @note Not decoration. It states at a glance, without a word of text, which
         * direction each player has to connect in.
         */
        sf::Color edge_red{232, 106, 106};

        /** @brief Blue's edges: the left and right sides of the board. */
        sf::Color edge_blue{106, 150, 232};

        /** @brief Row and column labels. */
        sf::Color label{124, 132, 148};

        /** @brief Primary line of the status bar. */
        sf::Color hud_primary{226, 230, 238};

        /** @brief Secondary lines of the status bar. */
        sf::Color hud_secondary{140, 148, 163};

        // --- Interface ---

        /** @brief Fill of a widget at rest. */
        sf::Color ui_fill{44, 49, 61};

        /** @brief Fill under the pointer. */
        sf::Color ui_fill_hover{58, 65, 80};

        /** @brief Fill of a pressed or focused widget. */
        sf::Color ui_fill_active{72, 81, 100};

        /** @brief Fill of a disabled widget. */
        sf::Color ui_fill_disabled{32, 36, 45};

        /** @brief Widget outline. */
        sf::Color ui_border{78, 86, 104};

        /** @brief Widget text. */
        sf::Color ui_text{226, 230, 238};

        /** @brief Text of a disabled widget, or placeholder text. */
        sf::Color ui_text_muted{112, 120, 136};

        /** @brief Colour of the selected option and of the accents. */
        sf::Color ui_accent{232, 106, 106};

        /**
         * @brief The accent on a filled surface.
         * @note Deeper than ui_accent, which is designed as an outline colour: the
         * same tint spread over sixty pixels of height reads as candy.
         */
        sf::Color accent_deep{198, 72, 72};

        /** @brief The accent under the pointer: the same, brighter. */
        sf::Color accent_bright{224, 92, 92};

        /**
         * @brief Surface of the flat widgets.
         * @note Closer to the background than ui_fill: in the flat style a widget
         * should barely surface from the ground rather than sit on it as a block.
         */
        sf::Color ui_surface{30, 34, 43};

        /** @brief Barely perceptible border, for continuous outlines. */
        sf::Color ui_border_soft{58, 64, 78};

        /** @brief Opacity of the ghost stone shown under the pointer. */
        std::uint8_t ghost_alpha{130};

        /** @brief Colour of the glow travelling along the winning chain. */
        sf::Color win_glow{255, 236, 168};

        /**
         * @brief Colour of the symbols drawn over the stones in colourblind mode.
         * @note Light, so it separates from red and blue alike.
         */
        sf::Color symbol{247, 249, 252};

        /**
         * @brief Whether stones carry a distinguishing geometric symbol.
         *
         * With it on, Red and Blue differ in shape as well as in hue. Under
         * deuteranopia and protanopia the two fills converge until they are
         * indistinguishable, leaving the board unreadable exactly when it needs
         * reading; a triangle and a diamond do not have that problem.
         *
         * @note Not a colour, but it lives here because the theme is the one thing
         * the renderer already receives from every screen. A separate channel for a
         * single boolean would mean threading it through every constructor by hand.
         */
        bool colorblind{false};

        /** @brief Thickness of the directional edges, in fractions of the circumradius. */
        float border_thickness{0.24f};

        /** @brief Thickness of the cell outline, in fractions of the circumradius. */
        float outline_thickness{0.05f};

        /** @brief Returns the default dark theme. */
        [[nodiscard]] static Theme dark() { return {}; }
    };

    /**
     * @brief Tints the theme with the chosen palette.
     *
     * Touches six colours and no others: the two stones, the two directional edges
     * and the two interface accents. That is the working definition of "board
     * style": it changes what colour the players are and what echoes them, not how
     * dark the panels are or how legible the text is. A theme that retints
     * everything stops being a theme and becomes a second design to maintain.
     *
     * @param theme Theme to modify in place.
     * @param palette Palette to apply; CLASSIC leaves it untouched.
     * @note Leaves `colorblind` alone: the symbols over the stones serve players who
     * cannot separate hues, and no palette makes them redundant. Gold and silver are
     * in fact closer to each other than red and blue.
     */
    inline void applyPalette(Theme& theme, const hexapp::BoardPalette palette) {
        switch (palette) {
            case hexapp::BoardPalette::TOXIC:
                // Neon green against purple: two hues that stay apart under every
                // form of colour blindness, which makes them a better pairing than
                // red and blue even for unimpaired vision.
                theme.piece_red  = {112, 232, 74};
                theme.piece_blue = {170, 92, 236};
                theme.edge_red   = {140, 244, 100};
                theme.edge_blue  = {192, 124, 246};
                theme.ui_accent     = {112, 232, 74};
                theme.accent_deep   = {84, 196, 52};
                theme.accent_bright = {138, 244, 104};
                break;

            case hexapp::BoardPalette::PRESTIGE:
                // Gold and silver are both light tones and separate by warmth rather
                // than by brightness. Hence the silver leans blue instead of neutral
                // grey, which beside the gold would read as dull gold.
                theme.piece_red  = {226, 178, 74};
                theme.piece_blue = {186, 198, 214};
                theme.edge_red   = {240, 198, 104};
                theme.edge_blue  = {212, 222, 236};
                theme.ui_accent     = {226, 178, 74};
                theme.accent_deep   = {192, 146, 52};
                theme.accent_bright = {240, 198, 104};
                break;

            case hexapp::BoardPalette::CLASSIC:
            default:
                break;   // i valori predefiniti sono gia' il classico
        }
    }
}

#endif //SFML_THEME_H
