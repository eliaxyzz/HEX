/**
 * @file test_console_view.cpp
 * @brief Text rendering and formatting.
 */

#include "ui/console_renderer.h"
#include "core/game_controller.h"
#include "core/game_ruler.h"
#include "core/hex_geometry.h"
#include "ui/sfml_game_observer.h"
#include "ui/sfml_human_player.h"
#include "core/test_players.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <set>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "test_doubles.h"
#include "test_framework.h"

using namespace hex;
using namespace hextest;

void console_view() {
    // --- The text view ---
    {
        // formatBoard is a pure function, checkable without capturing std::cout.
        const HexBoard empty3(3);
        const std::string expected =
            "    A  B  C \n"
            "1   .  .  . \n"
            "  2   .  .  . \n"
            "    3   .  .  . \n";
        CHECK(ConsoleRenderer::formatBoard(empty3) == expected, "formatBoard: board 3x3 vuota");

        HexBoard b(3);
        b.addPiece(Piece::RED_DISC, {0,0});
        b.addPiece(Piece::BLUE_DISC, {1,2});
        CHECK(ConsoleRenderer::formatBoard(b).find(" X ") != std::string::npos, "formatBoard: pedina rossa come X");
        CHECK(ConsoleRenderer::formatBoard(b).find(" O ") != std::string::npos, "formatBoard: pedina blu come O");
        CHECK(ConsoleRenderer::formatBoard(b, {0,0}).find("(X)") != std::string::npos, "formatBoard: highlight ultima mossa");
        CHECK(ConsoleRenderer::formatBoard(b).find("(X)") == std::string::npos, "formatBoard: nessun highlight di default");

        // render() writes exactly what formatBoard composes, to the given stream.
        std::ostringstream os;
        const ConsoleRenderer view(os);
        view.render(b, {0,0});
        CHECK(os.str() == ConsoleRenderer::formatBoard(b, {0,0}), "render() scrive su uno stream arbitrario");

        CHECK(ConsoleRenderer::formatMove(add(Player::RED, {0,0})) == "A1", "formatMove: {0,0} -> A1");
        CHECK(ConsoleRenderer::formatMove(add(Player::BLUE, {2,2})) == "C3", "formatMove: {2,2} -> C3");
        CHECK(ConsoleRenderer::formatMove(Move{}) == "RESIGN", "formatMove: resa");
        CHECK(ConsoleRenderer::formatPlayer(Player::RED) == "ROSSO", "formatPlayer: Rosso");
        CHECK(ConsoleRenderer::formatPlayer(Player::BLUE) == "BLU", "formatPlayer: Blu");
        CHECK(ConsoleRenderer::formatEndReason(EndReason::TIMEOUT) == "tempo scaduto", "formatEndReason: timeout");
    }
}
