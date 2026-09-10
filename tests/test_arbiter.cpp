/**
 * @file test_arbiter.cpp
 * @brief HexGameRuler: timeout, resignation, complete match.
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

void arbiter() {
    // --- Regression: referee timeout ---
    {
        SlowPlayer slow;
        test_players::RandomPlayer fast;
        const HexGameRuler ruler(11);
        const auto t0 = std::chrono::steady_clock::now();
        RecordingObserver rec;
        const GameResult res = ruler.play(slow, fast, 1, &rec);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        CHECK(res.winner == Player::BLUE, "il timeout del Rosso assegna la vittoria al Blu");
        CHECK(res.reason == EndReason::TIMEOUT, "l'arbitro riporta il motivo TIMEOUT");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "timeout") == 1, "observer: onTimeout notificato");
        CHECK(ms < 3000, "play() ritorna appena il worker cooperativo si ferma");
    }

    // --- Resignation as the referee sees it ---
    {
        ResignPlayer quitter;
        test_players::RandomPlayer other;
        const HexGameRuler ruler(11);
        const GameResult res = ruler.play(quitter, other, 5);
        CHECK(res.winner == Player::BLUE, "la resa del Rosso fa vincere il Blu");
        CHECK(res.reason == EndReason::RESIGN, "l'arbitro riporta il motivo RESIGN");
    }

    // --- Non-regression: a complete match ---
    {
        test_players::RandomPlayer a;
        test_players::SmartRandomPlayer b;
        const HexGameRuler ruler(11);
        const GameResult res = ruler.play(a, b, 5);
        CHECK(res.reason == EndReason::CONNECTION, "Random vs SmartRandom finisce per connessione");
        CHECK(res.winner == Player::RED || res.winner == Player::BLUE, "c'e' un vincitore");
    }
}
