/**
 * @file sfml_widgets.h
 * @brief Texture-based widget drawing with SFML.
 *
 * Free functions, stateless: the widgets live elsewhere and this only says how
 * they look. That is what keeps ui_widgets.h verifiable headless.
 *
 * Boxes are drawn with the nine-slice technique: the texture's corners keep their
 * size, the sides stretch along one axis and the centre stretches along both. That
 * is what lets one PNG dress a narrow button and a wide one without distorting the
 * rounded corners.
 */

#ifndef SFML_WIDGETS_H
#define SFML_WIDGETS_H

#include <SFML/Graphics.hpp>

#include <string>

#include <vector>

#include "ui/asset_manager.h"
#include "ui/sfml_theme.h"
#include "ui/ui_icons.h"
#include "ui/ui_widgets.h"

namespace hexgui {

    /**
     * @brief Draws a texture into a rectangle using nine-slice scaling.
     * @param target Destination surface.
     * @param texture Source texture, square.
     * @param dest Rectangle to fill.
     * @param border Thickness in pixels of the undistorted border.
     * @param tint Modulation colour; the alpha channel drives the fades.
     */
    void drawNineSlice(sf::RenderTarget& target, const sf::Texture& texture,
                       const hexui::Rect& dest, int border, sf::Color tint = sf::Color::White);

    /** @brief Draws a button. */
    void drawButton(sf::RenderTarget& target, const hexui::Button& button,
                    hexassets::AssetManager& assets, const Theme& theme, float opacity = 1.0f);

    /** @brief Draws a text field, with placeholder and blinking caret. */
    void drawTextInput(sf::RenderTarget& target, const hexui::TextInput& input,
                       hexassets::AssetManager& assets, const Theme& theme, float opacity = 1.0f);

    /** @brief Draws an option group, highlighting the selected entry. */
    void drawOptionGroup(sf::RenderTarget& target, const hexui::OptionGroup& group,
                         hexassets::AssetManager& assets, const Theme& theme, float opacity = 1.0f);

    // --- Flat style -----------------------------------------------------------
    //
    // The drawing below uses no textures: it builds the shapes on the spot. That is
    // what allows thin borders, continuous corners and soft fills without depending
    // on a PNG and without the staircase that scaling an image brings with it.

    /**
     * @brief Draws a rectangle with rounded corners.
     *
     * @param radius Corner radius, clamped to half the short side, past which the
     * arcs would overlap and the shape would fold in on itself.
     * @param outline_thickness Border thickness; zero draws the fill only.
     */
    void drawRoundedRect(sf::RenderTarget& target, const hexui::Rect& box, float radius,
                         sf::Color fill, sf::Color outline = sf::Color::Transparent,
                         float outline_thickness = 0.0f);

    /** @brief Visual weight of a command. */
    enum class ButtonStyle {
        PRIMARY,   ///< L'azione principale della schermata: piena e accesa.
        SECONDARY, ///< Un'azione fra le altre: superficie tenue e bordo sottile.
        GHOST      ///< Un'azione di servizio: solo il bordo.
    };

    /**
     * @brief Draws a button in the flat style, with an optional icon.
     * @note Icon and label are centred as one group, which stays in the middle of
     * the button whatever the text length. That is what keeps a row of commands
     * aligned across translations.
     */
    void drawFlatButton(sf::RenderTarget& target, const hexui::Button& button,
                        const sf::Font* font, const Theme& theme,
                        ButtonStyle style = ButtonStyle::SECONDARY,
                        Icon icon = Icon::NONE, float opacity = 1.0f);

    /**
     * @brief Draws an option group as a single pill-shaped control.
     *
     * One continuous shape contains every option, and the selected one is a soft
     * fill sliding inside it. That is the difference between a switch and three
     * buttons that happen to touch: it reads as one choice among alternatives rather
     * than three independent actions.
     *
     * @param icons One icon per option, or empty for none.
     */
    void drawSegmented(sf::RenderTarget& target, const hexui::OptionGroup& group,
                       const sf::Font* font, const Theme& theme,
                       const std::vector<Icon>& icons = {}, float opacity = 1.0f);

    /** @brief Draws a text field in the flat style. */
    void drawFlatTextInput(sf::RenderTarget& target, const hexui::TextInput& input,
                           const sf::Font* font, const Theme& theme,
                           Icon icon = Icon::NONE, float opacity = 1.0f);

    /**
     * @brief Draws text with a glow around it.
     * @note The glow is the text itself redrawn several times, slightly offset and
     * at low opacity. Not a real blur, but it stands in for one on screen and costs
     * four draws instead of a post-processing pass.
     */
    void drawGlowText(sf::RenderTarget& target, const std::string& text,
                      const hexui::Rect& area, const sf::Font* font, unsigned char_size,
                      sf::Color colour, sf::Color glow, float radius = 6.0f);

    /** @brief Draws one line of text centred in the given rectangle. */
    void drawCenteredText(sf::RenderTarget& target, const std::string& text,
                          const hexui::Rect& area, const sf::Font* font,
                          unsigned char_size, sf::Color color);

    /**
     * @brief Draws centred text across several lines within the given width.
     *
     * The text is broken between words by measuring the real width in the font in
     * use rather than by counting characters: a phrase and its translation differ in
     * length, and a fixed limit would suit only one of them.
     *
     * Lines are laid out from the top edge of the area. A word wider than the area
     * still occupies a line of its own: truncating it would render illegible
     * precisely the word that did not fit.
     *
     * @param area Width to wrap within; the height only positions the block.
     * @return Number of lines drawn.
     */
    std::size_t drawWrappedText(sf::RenderTarget& target, const std::string& text,
                                const hexui::Rect& area, const sf::Font* font,
                                unsigned char_size, sf::Color color);

    /** @brief Applies an opacity factor to a colour. */
    [[nodiscard]] sf::Color withOpacity(sf::Color color, float opacity);
}

#endif //SFML_WIDGETS_H
