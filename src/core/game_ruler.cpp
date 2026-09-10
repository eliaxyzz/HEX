/**
 * @file game_ruler.cpp
 * @brief Referee implementation: delegates everything to GameController.
 */

#include "core/game_ruler.h"
#include "core/game_controller.h"

namespace hex {

    GameResult HexGameRuler::play(AbstractPlayer& p1, AbstractPlayer& p2, const int sec,
                                  GameObserver* observer) const {
        GameController controller(size, p1, p2, sec);
        if (observer) controller.addObserver(*observer);
        return controller.run();
    }
}
