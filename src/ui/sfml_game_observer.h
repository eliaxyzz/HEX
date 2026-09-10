/**
 * @file sfml_game_observer.h
 * @brief Observer translating engine events into state for the graphical view.
 */

#ifndef SFML_GAME_OBSERVER_H
#define SFML_GAME_OBSERVER_H

#include <optional>
#include <string>
#include <utility>

#include "core/game.h"
#include "core/game_observer.h"
#include "core/localization.h"
#include "core/move.h"

namespace hexgui {

    /**
     * @brief Visual state of the match, driven by the controller's events.
     */
    class SfmlGameObserver final : public hex::GameObserver {
    public:
        /**
         * @brief Builds the observer in the given language.
         * @param loc Interface strings.
         * @warning Must outlive the observer: phrases are composed when the event
         * arrives, not when they are read.
         */
        explicit SfmlGameObserver(const hexui::LocalizationManager& loc) : loc(loc) {}

        // --- GameObserver ---

        void onGameStart(const hex::Situation& initial) override;
        void onTurnStart(const hex::Situation& current, hex::Player mover,
                         const std::string& playerName) override;
        void onMove(const hex::Situation& before, const hex::Move& m,
                    const hex::Situation& after) override;
        void onUndo(const hex::Situation& restored, const std::optional<hex::Move>& undone) override;
        void onInvalidMove(const hex::Situation& current, hex::Player mover, const hex::Move& m,
                           const std::string& playerName) override;
        void onTimeout(const hex::Situation& current, hex::Player mover,
                       const std::string& playerName) override;
        void onPlayerError(const hex::Situation& current, hex::Player mover,
                           const std::string& playerName, const std::string& what) override;
        void onGameEnd(const hex::Situation& final, const hex::GameResult& result) override;

        // --- State read by the renderer ---

        /** @brief Tests whether anything changed since the last draw. */
        [[nodiscard]] bool isDirty() const { return dirty; }

        /** @brief Marks the view as drawn. */
        void clearDirty() { dirty = false; }

        /** @brief Cell of the last move, to highlight. Empty after a swap or a resignation. */
        [[nodiscard]] const std::optional<std::pair<int, int>>& lastMove() const { return last_move; }

        /** @brief Status line to display: whose turn it is, or how the match ended. */
        [[nodiscard]] const std::string& statusText() const { return status; }

        /** @brief Secondary message for an irregular ending: timeout, illegal move, error. */
        [[nodiscard]] const std::string& noticeText() const { return notice; }

        /** @brief Match outcome, available only once the match has ended. */
        [[nodiscard]] const std::optional<hex::GameResult>& result() const { return outcome; }

        /** @brief Name of the side to move, for the status line. */
        [[nodiscard]] const std::string& currentPlayerName() const { return current_player; }

    private:
        /** @brief True when the renderer must redraw. */
        bool dirty = true;

        /** @brief Cell to highlight. */
        std::optional<std::pair<int, int>> last_move;

        /** @brief Primary status line. */
        std::string status;

        /** @brief Secondary line for irregular endings. */
        std::string notice;

        /** @brief Name of the side to move. */
        std::string current_player;

        /** @brief Interface strings in the selected language. Non-owning. */
        const hexui::LocalizationManager& loc;

        /** @brief Final outcome. */
        std::optional<hex::GameResult> outcome;
    };
}

#endif //SFML_GAME_OBSERVER_H
