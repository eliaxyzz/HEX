/**
 * @file sfml_input.cpp
 * @brief Event translation implementation.
 */

#include "ui/sfml_input.h"

namespace hexapp {

    namespace {
        /** @brief Maps the keys the application uses; the rest stay UNKNOWN. */
        Key translateKey(const sf::Keyboard::Key code) {
            switch (code) {
                case sf::Keyboard::Key::Enter:     return Key::ENTER;
                case sf::Keyboard::Key::Escape:    return Key::ESCAPE;
                case sf::Keyboard::Key::Backspace: return Key::BACKSPACE;
                case sf::Keyboard::Key::S:         return Key::S;
                case sf::Keyboard::Key::R:         return Key::R;
                default:                           return Key::UNKNOWN;
            }
        }

        MouseButton translateButton(const sf::Mouse::Button b) {
            switch (b) {
                case sf::Mouse::Button::Left:  return MouseButton::LEFT;
                case sf::Mouse::Button::Right: return MouseButton::RIGHT;
                default:                       return MouseButton::OTHER;
            }
        }
    }

    std::optional<InputEvent> translateEvent(const sf::Event& event, const sf::RenderWindow& window) {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            const Key k = translateKey(key->code);
            if (k == Key::UNKNOWN) return std::nullopt;
            return InputEvent::keyPress(k);
        }

        if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
            const sf::Vector2f p = window.mapPixelToCoords(moved->position);
            return InputEvent::mouseMove(p.x, p.y);
        }

        if (const auto* click = event.getIf<sf::Event::MouseButtonPressed>()) {
            const sf::Vector2f p = window.mapPixelToCoords(click->position);
            return InputEvent::mousePress(p.x, p.y, translateButton(click->button));
        }

        if (event.is<sf::Event::MouseLeft>()) {
            InputEvent e;
            e.type = InputType::MOUSE_LEFT;
            return e;
        }

        if (const auto* typed = event.getIf<sf::Event::TextEntered>()) {
            InputEvent e;
            e.type = InputType::TEXT_ENTERED;
            e.text = typed->unicode;
            return e;
        }

        if (const auto* resized = event.getIf<sf::Event::Resized>()) {
            return InputEvent::resized(static_cast<float>(resized->size.x),
                                       static_cast<float>(resized->size.y));
        }

        return std::nullopt;   // Closed e il resto sono gestiti dall'applicazione
    }
}
