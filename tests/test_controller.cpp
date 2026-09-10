/**
 * @file test_controller.cpp
 * @brief Step-driven GameController and observer notifications.
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

void controller() {
    // --- Step-driven GameController and GameObserver ---
    {
        // step() advances one turn and hands control back to the caller.
        test_players::RandomPlayer a, b;
        GameController c(11, a, b);
        CHECK(!c.isOver(), "controller: partita aperta alla creazione");
        CHECK(c.result() == std::nullopt, "controller: nessun risultato prima della fine");
        CHECK(c.situation().toMove() == Player::RED, "controller: muove il Rosso");

        CHECK(stepUntilSettled(c) == StepResult::PLAYED, "step() gioca un turno");
        CHECK(c.situation().toMove() == Player::BLUE, "step(): un solo turno per chiamata");
        CHECK(c.situation().getBoard().getPosByPiece(Piece::RED_DISC).size() == 1,
              "step(): esattamente una pedina piazzata");
    }
    {
        // Event sequence over a complete match.
        test_players::RandomPlayer a, b;
        GameController c(11, a, b);
        RecordingObserver rec;
        c.addObserver(rec);
        const GameResult res = c.run();

        CHECK(rec.events.front() == "start", "observer: onGameStart e' il primo evento");
        CHECK(rec.events.back() == "end", "observer: onGameEnd e' l'ultimo evento");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "start") == 1, "observer: un solo onGameStart");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "end") == 1, "observer: un solo onGameEnd");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "turn") == rec.moves,
              "observer: un onTurnStart per ogni onMove");
        CHECK(std::count(rec.events.begin(), rec.events.end(), "BUG:before==after") == 0,
              "observer: onMove riceve stati before/after distinti");
        CHECK(rec.final_result && rec.final_result->winner == res.winner,
              "observer: onGameEnd riporta lo stesso esito di run()");
        CHECK(res.reason == EndReason::CONNECTION, "partita completa: vittoria per connessione");
        CHECK(c.step() == StepResult::GAME_OVER, "step() su partita finita ritorna GAME_OVER");
    }
    {
        // Illegal move: event reported, match closed, no output from the engine.
        IllegalPlayer bad;
        test_players::RandomPlayer good;
        GameController c(11, bad, good);
        RecordingObserver rec;
        c.addObserver(rec);
        const GameResult res = c.run();
        CHECK(std::count(rec.events.begin(), rec.events.end(), "invalid") == 1, "observer: onInvalidMove notificato");
        CHECK(res.winner == Player::BLUE, "mossa illegale del Rosso: vince il Blu");
        CHECK(res.reason == EndReason::ILLEGAL_MOVE, "motivo: ILLEGAL_MOVE");
    }
    {
        // Player exception: a dedicated event, and the process does not terminate.
        ThrowingPlayer bad;
        test_players::RandomPlayer good;
        GameController c(11, bad, good);
        RecordingObserver rec;
        c.addObserver(rec);
        const GameResult res = c.run();
        CHECK(std::count(rec.events.begin(), rec.events.end(), "error") == 1, "observer: onPlayerError notificato");
        CHECK(res.winner == Player::BLUE, "eccezione del Rosso: vince il Blu");
    }
    {
        // The console view is a plain client: it writes to an arbitrary stream.
        test_players::RandomPlayer a, b;
        GameController c(11, a, b);
        std::ostringstream os;
        ConsoleGameObserver view("Rosso1", "Blu2", os);
        c.addObserver(view);
        c.run();
        const std::string text = os.str();
        CHECK(text.find("--> Turno di ROSSO") != std::string::npos, "console: annuncio del turno");
        CHECK(text.find("Mossa scelta: ") != std::string::npos, "console: mossa scelta");
        CHECK(text.find("!!! VINCITORE: ") != std::string::npos, "console: annuncio del vincitore");
        CHECK(text.find("[connessione]") != std::string::npos, "console: motivo della vittoria");
        CHECK(text.find(" A  B  C ") != std::string::npos, "console: la scacchiera viene disegnata");
    }
}
