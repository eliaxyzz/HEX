/**
 * @file sfml_human_player.cpp
 * @brief Human player implementation.
 */

#include "ui/sfml_human_player.h"

#include <utility>

namespace hexgui {

    SfmlHumanPlayer::SfmlHumanPlayer(std::string name) : DeferredPlayer(std::move(name)) {}

    hex::Move SfmlHumanPlayer::placementAt(const std::pair<int, int> pos) const {
        // The turn decides the colour, not the click, so there is no way to play a
        // stone of the wrong colour.
        return hex::Move{hex::MoveKind::ADD,
                         hex::Action(hex::ActionKind::ADD,
                                     pieceOf(askedSituation().toMove()), pos)};
    }

    bool SfmlHumanPlayer::wouldAccept(const std::pair<int, int> pos) const {
        return DeferredPlayer::wouldAccept(placementAt(pos));
    }

    bool SfmlHumanPlayer::onCellClicked(const std::pair<int, int> pos) {
        return submit(placementAt(pos));
    }

    bool SfmlHumanPlayer::onSwapRequested() {
        if (!swapMove()) return false;

        return submit(*swapMove());
    }
}
