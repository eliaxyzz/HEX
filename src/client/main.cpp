/**
 * @file main.cpp
 * @brief Entry point of the console demo: engine-versus-engine exhibition matches.
 */
#include <chrono>
#include <iostream>
#include <thread>
#include <string>

#include "ui/console_renderer.h"
#include "core/game_controller.h"
#include "core/player.h"
#include "core/test_players.h"


/**
 * @brief Plays one full match with console output.
 *
 * Owns no game loop: it builds a GameController, attaches the console view as an
 * observer and lets it run. Everything printed comes from the events the
 * controller emits.
 *
 * @param p1 First player; plays Red and moves first.
 * @param p2 Second player; plays Blue.
 * @param title Descriptive match title for the banner.
 */
void playVisualMatch(hex::AbstractPlayer& p1, hex::AbstractPlayer& p2, const std::string& title) {
    std::cout << "\n\n";
    std::cout << "##################################################\n";
    std::cout << "       INIZIO MATCH: " << title << "\n";
    std::cout << "       " << p1.getName() << " (Rosso) vs " << p2.getName() << " (Blu)\n";
    std::cout << "##################################################\n\n";

    hex::GameController controller(11, p1, p2);
    hex::ConsoleGameObserver view(p1.getName(), p2.getName());
    controller.addObserver(view);

    controller.run();
}

int main() {
    hex::HexPlayer mcts_bot_1;
    hex::HexPlayer mcts_bot_2;

    test_players::RandomPlayer random_p;       // moves at random
    test_players::SmartRandomPlayer smart_p;   // converts immediate wins

    // Every pairing is played twice with the colours swapped, since Red's
    // first-move advantage makes a single game an unreliable comparison.

    // MCTS against the random baseline.
    playVisualMatch(mcts_bot_1, random_p, "MCTS Player vs RANDOM Player");
    playVisualMatch(random_p, mcts_bot_1, "RANDOM Player vs MCTS Player");

    std::cout << "\n...pausa tattica (3s)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // MCTS against the greedy baseline.
    playVisualMatch(mcts_bot_1, smart_p, "MCTS Player vs SMART RANDOM Player");
    playVisualMatch(smart_p, mcts_bot_1, "SMART RANDOM Player vs MCTS Player");

    std::cout << "\n...pausa tattica (3s)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // MCTS against itself.
    playVisualMatch(mcts_bot_1, mcts_bot_2, "MCTS1 vs MCTS2 (Andata)");
    playVisualMatch(mcts_bot_2, mcts_bot_1, "MCTS2 vs MCTS1 (Ritorno)");

    std::cout << "\n\nTUTTI I MATCH COMPLETATI.\n";

    return 0;
}
