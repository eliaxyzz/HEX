/**
 * @file app_state.cpp
 * @brief Navigation rules between screens.
 */

#include "states/app_state.h"

namespace hexapp {

    std::optional<Transition> menuTransitionFor(const InputEvent& event) {
        if (event.type != InputType::KEY_PRESSED) return std::nullopt;

        switch (event.key) {
            case Key::ENTER:  return Transition::to(StateId::PLAYING);
            case Key::ESCAPE: return Transition::quit();
            default:          return std::nullopt;
        }
    }

    std::optional<Transition> playingTransitionFor(const InputEvent& event) {
        if (event.type != InputType::KEY_PRESSED) return std::nullopt;

        // Escape returns to the menu rather than quitting: leaving a running match
        // by accident costs far more than pressing Escape twice.
        if (event.key == Key::ESCAPE) return Transition::to(StateId::MAIN_MENU);
        return std::nullopt;
    }
}
