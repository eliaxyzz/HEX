/**
 * @file sfml_input.h
 * @brief Translation of SFML events into the neutral form the states consume.
 *
 * @note The single place where the application knows SFML's event types. Past
 * this boundary the logic works on InputEvent and is verifiable without opening a
 * window.
 */

#ifndef SFML_INPUT_H
#define SFML_INPUT_H

#include <SFML/Graphics.hpp>

#include <optional>

#include "states/app_state.h"

namespace hexapp {

    /**
     * @brief Converts an SFML event into an InputEvent.
     * @param event Window event.
     * @param window Window, used to map pixels into view coordinates.
     * @return The translated event, or nullopt for events of no interest.
     */
    [[nodiscard]] std::optional<InputEvent> translateEvent(const sf::Event& event,
                                                           const sf::RenderWindow& window);
}

#endif //SFML_INPUT_H
