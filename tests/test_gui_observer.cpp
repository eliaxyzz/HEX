/**
 * @file test_gui_observer.cpp
 * @brief SfmlGameObserver: visual state.
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
#include "core/localization.h"
#include "test_framework.h"

using namespace hex;
using namespace hextest;

void gui_observer() {
    // --- SfmlGameObserver ---
    {
        using hexgui::SfmlGameObserver;

        // Engine versus engine: the visual state follows the events.
        {
            test_players::RandomPlayer a, b;
            GameController c(11, a, b);
            const hexui::LocalizationManager loc;
            SfmlGameObserver view(loc);
            c.addObserver(view);

            c.step();
            CHECK(view.isDirty(), "observer: sporco dopo l'avvio");
            CHECK(view.statusText().find("Turno:") != std::string::npos, "observer: annuncia il turno");
            CHECK(view.result() == std::nullopt, "observer: nessun esito a partita in corso");

            view.clearDirty();
            CHECK(!view.isDirty(), "observer: clearDirty pulisce il flag");

            stepUntilSettled(c);
            CHECK(view.isDirty(), "observer: una mossa sporca di nuovo la vista");
            CHECK(view.lastMove().has_value(), "observer: registra la cella dell'ultima mossa");
            CHECK(view.lastMove() == c.moveHistory().back().action.position,
                  "observer: la cella evidenziata e' quella giocata");

            const GameResult res = c.run();
            CHECK(view.result().has_value(), "observer: a fine partita l'esito e' disponibile");
            CHECK(view.result()->winner == res.winner, "observer: l'esito coincide con quello del controller");
            CHECK(view.statusText().find("Ha vinto:") != std::string::npos, "observer: annuncia il vincitore");
            CHECK(view.statusText().find("connessione") != std::string::npos,
                  "observer: la riga di stato dice come e' finita");
        }

        // Undo: the outcome is withdrawn and the view returns to in-progress,
        // silently. An undo is how a restart walks the history back; it is not a user
        // action and must not produce overlay text.
        {
            DeferredPlayer red, blue;
            GameController c(11, red, blue);
            const hexui::LocalizationManager loc;
            SfmlGameObserver view(loc);
            c.addObserver(view);

            for (int r = 0; r < 10; ++r) {
                red.submit(add(Player::RED, {r, 0}));
                stepUntilSettled(c);
                blue.submit(add(Player::BLUE, {r, 5}));
                stepUntilSettled(c);
            }
            red.submit(add(Player::RED, {10, 0}));
            stepUntilSettled(c);
            CHECK(view.result().has_value(), "observer: esito registrato a fine partita");

            c.undo();
            CHECK(view.result() == std::nullopt, "observer: l'undo ritira l'esito");
            CHECK(!view.lastMove().has_value(), "observer: l'undo toglie l'evidenziazione");
            CHECK(view.noticeText().empty(),
                  "observer: l'undo non lascia messaggi a schermo");
        }

        // Timeouts and the pie rule land in the secondary line.
        {
            DeferredPlayer silent;
            test_players::RandomPlayer bot;
            GameController c(11, silent, bot, 1);
            const hexui::LocalizationManager loc;
            SfmlGameObserver view(loc);
            c.addObserver(view);
            c.run();
            CHECK(view.noticeText().find("Tempo scaduto") != std::string::npos, "observer: segnala il timeout");
            CHECK(view.result()->reason == EndReason::TIMEOUT, "observer: esito per timeout");
        }
        {
            DeferredPlayer red, blue;
            GameController c(11, red, blue);
            const hexui::LocalizationManager loc;
            SfmlGameObserver view(loc);
            c.addObserver(view);

            red.submit(add(Player::RED, {2, 5}));
            stepUntilSettled(c);
            blue.submit(Move(MoveKind::PIE, Action(ActionKind::SWAP, Piece::BLUE_DISC, {2, 5})));
            stepUntilSettled(c);

            CHECK(view.noticeText().find("Pie Rule") != std::string::npos, "observer: segnala lo scambio");
            CHECK(!view.lastMove().has_value(), "observer: lo scambio non evidenzia una cella");
        }
    }
}
