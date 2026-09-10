/**
 * @file ui_icons.h
 * @brief Minimal icons drawn from geometric primitives.
 *
 * ### Why not emoji
 *
 * The interface font (assets/ui.ttf, Roboto Medium) covers 927 code points: Latin,
 * punctuation and little else. It contains no emoji, and no arrows or diamonds
 * either. SFML does not fall back to another font when a glyph is missing, so an
 * emoji in a label does not become a picture: it becomes nothing, or an empty box.
 *
 * Adding an emoji font would not help. The system ones, Segoe UI Emoji and its
 * counterparts, are colour fonts, a format SFML does not render, and they do not
 * exist under the same name outside Windows.
 *
 * The icons are therefore shapes: a few circles, rectangles and lines. They cost
 * little, scale to any size without blurring, take their colour from the theme,
 * and above all render on any machine whatever font the user drops into assets/.
 */

#ifndef UI_ICONS_H
#define UI_ICONS_H

#include <SFML/Graphics/RenderTarget.hpp>

#include "core/hex_geometry.h"

namespace hexgui {

    /** @brief The icons available to the interface. */
    enum class Icon {
        NONE,      ///< Nessuna icona: l'etichetta occupa tutto lo spazio.
        AI,        ///< Un volto squadrato con antenna: il motore.
        PEOPLE,    ///< Due figure affiancate: due umani.
        ONLINE,    ///< Uno schermo su una base: la partita in rete.
        SETTINGS,  ///< Un cerchio con i denti: le preferenze.
        FOLDER,    ///< Una cartella: il caricamento.
        EXIT,      ///< Una porta con la maniglia: l'uscita.
        LEVEL_1,   ///< Una barretta: il livello facile.
        LEVEL_2,   ///< Due barrette crescenti: il livello medio.
        LEVEL_3,   ///< Tre barrette crescenti: il livello difficile.
        PLAY,      ///< Un triangolo: l'azione principale.
        USER,      ///< Testa e spalle: il nome del giocatore.
        DISC_RED,  ///< Una pedina rossa: il colore Rosso.
        DISC_BLUE, ///< Una pedina blu: il colore Blu.
        HELP,      ///< Un punto interrogativo: il tutorial.
        TRASH,     ///< Un cestino con il coperchio: la cancellazione.
        PENCIL,    ///< Una matita inclinata: la rinomina.
        BLACK_HOLE ///< Un anello attorno al vuoto: la modalita' Arcade.
    };

    /**
     * @brief Draws the icon centred on the given point.
     *
     * @param target Destination surface.
     * @param icon Icon to draw; NONE draws nothing.
     * @param centre Icon centre, in pixels.
     * @param size Side of the square containing it.
     * @param colour Stroke colour; the alpha channel drives the fades.
     */
    void drawIcon(sf::RenderTarget& target, Icon icon, Point centre, float size, sf::Color colour);

    /** @brief Returns the width an icon occupies in a label, spacing included. */
    [[nodiscard]] float iconSlot(float size);
}

#endif //UI_ICONS_H
