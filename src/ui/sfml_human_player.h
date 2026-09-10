/**
 * @file sfml_human_player.h
 * @brief Human player: turns clicks into moves for the engine.
 *
 * @note The turn mechanics - input window, validation, queueing - are not here.
 * They live in hexplay::DeferredPlayer, shared with the remote player of the
 * network build. What remains here is the translation from a click on a cell to a
 * move, the only part specific to the graphical interface.
 */

#ifndef SFML_HUMAN_PLAYER_H
#define SFML_HUMAN_PLAYER_H

#include <string>
#include <utility>

#include "core/deferred_player.h"

namespace hexgui {

    /**
     * @brief The human player sitting at the window.
     *
     * The engine asks for a move through startMove() and collects it through
     * tryTakeMove(); any number of frames may pass in between, during which
     * tryTakeMove() answers nullopt and GameController::step() returns WAITING.
     *
     * @note A click is accepted only inside that window and only if the
     * corresponding move is legal; both rules belong to the base class.
     */
    class SfmlHumanPlayer final : public hexplay::DeferredPlayer {
    public:
        /**
         * @brief Builds the human player.
         * @param name Name shown in the interface and in logs.
         */
        explicit SfmlHumanPlayer(std::string name = "Umano");

        /**
         * @brief Tests whether a click on that cell would be accepted.
         * @note Lets the interface preview a move only where the click would really
         * work, so visual feedback and input share one rule.
         */
        [[nodiscard]] bool wouldAccept(std::pair<int, int> pos) const;

        /**
         * @brief Records a click on a cell.
         * @param pos Logical coordinates of the clicked cell.
         * @return true if the click was accepted and the move queued.
         */
        bool onCellClicked(std::pair<int, int> pos);

        /** @brief Tests whether the pie rule swap is playable on this turn. */
        [[nodiscard]] bool canSwap() const { return swapMove().has_value(); }

        /**
         * @brief Records a pie rule swap request.
         * @return true if the swap was available and has been queued.
         */
        bool onSwapRequested();

    private:
        /** @brief Returns the placement move a click maps to on the current turn. */
        [[nodiscard]] hex::Move placementAt(std::pair<int, int> pos) const;
    };
}

#endif //SFML_HUMAN_PLAYER_H
