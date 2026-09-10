/**
 * @file test_clock.cpp
 * @brief Match clock and turn order.
 *
 * The clock receives time from outside through tick(dt) rather than reading the
 * system clock, which is what allows ten minutes to pass in a microsecond and
 * expiry to be checked without actually waiting for it.
 *
 * The second half covers colour selection: who holds which colour, who therefore
 * moves first, and what the two players are called when the human picks Blue.
 */

#include "core/game_clock.h"

#include <optional>
#include <string>

#include "core/game_controller.h"
#include "core/game_settings.h"
#include "core/test_players.h"
#include "core/localization.h"
#include "test_framework.h"

using namespace hex;
using hexplay::GameClock;
using hexapp::GameSettings;
using hexapp::GameMode;
using hexapp::Difficulty;

/** @brief Default-language strings: the checks compare against that text. */
namespace { const hexui::LocalizationManager loc; }

void game_clock() {

    // --- Time runs only for the side to move ---
    {
        GameClock c(60.0);
        CHECK(c.enabled(), "orologio: attivo con un budget positivo");
        CHECK(c.remaining(Player::RED) == 60.0, "orologio: il Rosso parte col budget pieno");
        CHECK(c.remaining(Player::BLUE) == 60.0, "orologio: il Blu parte col budget pieno");

        // With no running player nothing elapses: that is the pause.
        c.tick(5.0);
        CHECK(c.remaining(Player::RED) == 60.0, "orologio: in pausa il Rosso non consuma");
        CHECK(c.remaining(Player::BLUE) == 60.0, "orologio: in pausa il Blu non consuma");

        c.setRunning(Player::RED);
        c.tick(10.0);
        CHECK(c.remaining(Player::RED) == 50.0, "orologio: il Rosso ha speso dieci secondi");
        CHECK(c.remaining(Player::BLUE) == 60.0, "orologio: il Blu non ha speso nulla");

        c.setRunning(Player::BLUE);
        c.tick(4.0);
        CHECK(c.remaining(Player::RED) == 50.0, "orologio: passato il turno, il Rosso si ferma");
        CHECK(c.remaining(Player::BLUE) == 56.0, "orologio: ora consuma il Blu");
    }
    {
        // A negative dt would hand time back: a clock does not run backwards.
        GameClock c(30.0);
        c.setRunning(Player::RED);
        c.tick(-5.0);
        CHECK(c.remaining(Player::RED) == 30.0, "orologio: un dt negativo non regala tempo");
    }
    {
        // The remainder stops at zero rather than going negative.
        GameClock c(3.0);
        c.setRunning(Player::RED);
        c.tick(10.0);
        CHECK(c.remaining(Player::RED) == 0.0, "orologio: il residuo non scende sotto zero");
    }

    // --- Expiry ---
    {
        GameClock c(2.0);
        CHECK(!c.expiredPlayer().has_value(), "scadenza: all'inizio nessuno e' scaduto");

        c.setRunning(Player::BLUE);
        c.tick(1.5);
        CHECK(!c.expired(Player::BLUE), "scadenza: con mezzo secondo residuo non e' scaduto");

        c.tick(0.5);
        CHECK(c.expired(Player::BLUE), "scadenza: esaurito il budget, e' scaduto");
        CHECK(c.expiredPlayer() == Player::BLUE, "scadenza: e' il Blu ad avere finito");
        CHECK(!c.expired(Player::RED), "scadenza: il Rosso ha ancora tempo");
    }
    {
        // A non-positive budget disables the clock, and nobody ever expires.
        GameClock c(0.0);
        CHECK(!c.enabled(), "senza limite: l'orologio e' disattivato");

        c.setRunning(Player::RED);
        c.tick(100000.0);
        CHECK(!c.expired(Player::RED), "senza limite: non si scade");
        CHECK(!c.expiredPlayer().has_value(), "senza limite: nessuno scaduto");
    }

    // --- Formatting ---
    {
        CHECK(GameClock::format(600.0) == "10:00", "formato: dieci minuti");
        CHECK(GameClock::format(59.0) == "00:59", "formato: sotto il minuto");
        CHECK(GameClock::format(60.0) == "01:00", "formato: un minuto esatto");
        CHECK(GameClock::format(0.0) == "00:00", "formato: tempo esaurito");
        CHECK(GameClock::format(-5.0) == "00:00", "formato: un residuo negativo vale zero");

        // Rounds up: while any fraction remains, it reads as one second.
        CHECK(GameClock::format(0.2) == "00:01", "formato: un frammento di secondo si vede");
        CHECK(GameClock::format(59.4) == "01:00", "formato: 59,4 secondi sono un minuto");
    }

    // --- A loss on time goes through the controller ---
    {
        // The clock reports, it does not referee: the controller closes the match,
        // with the same outcome as a per-move timeout.
        test_players::RandomPlayer a, b;
        GameController c(5, a, b);
        (void)c.step();   // la partita e' cominciata, il Rosso e' stato interrogato

        CHECK(!c.isOver(), "tempo scaduto: la partita e' in corso");
        CHECK(c.claimTimeout(Player::RED), "tempo scaduto: il controller accetta la dichiarazione");
        CHECK(c.isOver(), "tempo scaduto: la partita e' chiusa");

        const std::optional<GameResult> res = c.result();
        CHECK(res.has_value(), "tempo scaduto: c'e' un esito");
        CHECK(res->reason == EndReason::TIMEOUT, "tempo scaduto: il motivo e' il tempo");
        CHECK(res->winner == Player::BLUE, "tempo scaduto: vince l'avversario");
    }
    {
        // An already finished match is not closed twice.
        test_players::RandomPlayer a, b;
        GameController c(5, a, b);
        (void)c.run();

        CHECK(c.isOver(), "tempo scaduto: partita gia' finita");
        CHECK(!c.claimTimeout(Player::RED), "tempo scaduto: la dichiarazione tardiva viene rifiutata");
    }
    {
        // After the declaration no player is still thinking: the turn in flight is
        // closed, or destroying the players would be unsafe.
        test_players::RandomPlayer a, b;
        GameController c(5, a, b);
        (void)c.step();
        (void)c.claimTimeout(Player::RED);

        CHECK(a.tryTakeMove() == std::nullopt, "tempo scaduto: nessuna richiesta resta in volo");
    }

    // --- Colour selection: who moves first ---
    {
        // By default the human is Red and opens the match.
        GameSettings s;
        s.mode = GameMode::HUMAN_VS_AI;
        s.player_name = "Elia";

        CHECK(s.human_colour == Player::RED, "colore: di default l'umano e' il Rosso");
        CHECK(hexapp::isHumanColour(s, Player::RED), "colore: il Rosso e' umano");
        CHECK(!hexapp::isHumanColour(s, Player::BLUE), "colore: il Blu e' il motore");
        CHECK(hexapp::nameOfColour(s, Player::RED, loc) == "Elia", "colore: il Rosso ha il nickname");
        CHECK(hexapp::nameOfColour(s, Player::BLUE, loc) == "Computer Medio", "colore: il Blu e' l'IA");
    }
    {
        // Choosing Blue makes the engine the first to move.
        GameSettings s;
        s.mode = GameMode::HUMAN_VS_AI;
        s.player_name = "Elia";
        s.difficulty = Difficulty::HARD;
        s.human_colour = Player::BLUE;

        CHECK(!hexapp::isHumanColour(s, Player::RED), "colore Blu: il Rosso e' il motore");
        CHECK(hexapp::isHumanColour(s, Player::BLUE), "colore Blu: il Blu e' umano");
        CHECK(hexapp::nameOfColour(s, Player::RED, loc) == "Computer Difficile",
              "colore Blu: il primo a muovere e' l'IA");
        CHECK(hexapp::nameOfColour(s, Player::BLUE, loc) == "Elia",
              "colore Blu: l'umano resta se stesso");

        // Saved names follow the colour, not whoever configured the match.
        CHECK(hexapp::firstPlayerName(s, loc) == "Computer Difficile",
              "colore Blu: il nome del Rosso e' quello dell'IA");
        CHECK(hexapp::secondPlayerName(s, loc) == "Elia", "colore Blu: il nome del Blu e' l'umano");
    }
    {
        // Between two humans the colour is not chosen: the first name is Red.
        GameSettings s;
        s.mode = GameMode::HUMAN_VS_HUMAN;
        s.player_name = "Anna";
        s.opponent_name = "Bruno";
        s.human_colour = Player::BLUE;   // ignorato in questa modalita'

        CHECK(hexapp::isHumanColour(s, Player::RED), "due umani: il Rosso e' umano");
        CHECK(hexapp::isHumanColour(s, Player::BLUE), "due umani: anche il Blu");
        CHECK(hexapp::nameOfColour(s, Player::RED, loc) == "Anna", "due umani: il primo nome e' il Rosso");
        CHECK(hexapp::nameOfColour(s, Player::BLUE, loc) == "Bruno", "due umani: il secondo e' il Blu");
    }
    {
        // Names left blank: the match is played all the same.
        GameSettings s;
        s.mode = GameMode::HUMAN_VS_AI;
        s.human_colour = Player::BLUE;

        CHECK(hexapp::nameOfColour(s, Player::BLUE, loc) == "Umano", "ripiego: l'umano ha un nome");
        CHECK(!hexapp::nameOfColour(s, Player::RED, loc).empty(), "ripiego: anche l'IA ce l'ha");
    }

    // --- Rematch: the colours alternate ---
    //
    // Red moves first and that advantage is real: repeating the same assignment
    // would turn a series of matches into one match repeated.
    {
        // Against the engine the human's colour flips and the names follow by themselves.
        GameSettings s;
        s.mode = GameMode::HUMAN_VS_AI;
        s.player_name = "Elia";
        s.difficulty = Difficulty::HARD;
        s.human_colour = Player::RED;

        const GameSettings next = hexapp::rematchOf(s);

        CHECK(next.human_colour == Player::BLUE, "rivincita: l'umano passa al Blu");
        CHECK(!hexapp::isHumanColour(next, Player::RED), "rivincita: il Rosso ora e' il motore");
        CHECK(hexapp::isHumanColour(next, Player::BLUE), "rivincita: il Blu ora e' umano");
        CHECK(hexapp::nameOfColour(next, Player::BLUE, loc) == "Elia",
              "rivincita: l'umano resta se stesso, cambia colore");
        CHECK(hexapp::nameOfColour(next, Player::RED, loc) == "Computer Difficile",
              "rivincita: ora apre l'IA");

        // The rest of the configuration is unrelated and must not move.
        CHECK(next.mode == s.mode, "rivincita: stessa modalita'");
        CHECK(next.difficulty == s.difficulty, "rivincita: stesso livello");
        CHECK(next.board_size == s.board_size, "rivincita: stessa scacchiera");
        CHECK(next.player_name == s.player_name, "rivincita: stesso nickname");

        // Two rematches return to the starting point: this is an alternation, not a
        // drift. Across three consecutive matches a player opens two of them only if
        // they opened the first.
        CHECK(hexapp::rematchOf(next).human_colour == Player::RED,
              "rivincita: la rivincita della rivincita torna al colore iniziale");
    }
    {
        // Starting from Blue too: the alternation holds in both directions.
        GameSettings s;
        s.mode = GameMode::HUMAN_VS_AI;
        s.human_colour = Player::BLUE;

        CHECK(hexapp::rematchOf(s).human_colour == Player::RED,
              "rivincita: dal Blu si torna al Rosso");
    }
    {
        // Between two humans there is no human_colour to flip: the first name is Red,
        // so swapping colours means swapping names.
        GameSettings s;
        s.mode = GameMode::HUMAN_VS_HUMAN;
        s.player_name = "Anna";
        s.opponent_name = "Bruno";

        const GameSettings next = hexapp::rematchOf(s);

        CHECK(hexapp::nameOfColour(next, Player::RED, loc) == "Bruno",
              "rivincita fra umani: ora apre Bruno");
        CHECK(hexapp::nameOfColour(next, Player::BLUE, loc) == "Anna",
              "rivincita fra umani: Anna risponde");
        CHECK(hexapp::nameOfColour(hexapp::rematchOf(next), Player::RED, loc) == "Anna",
              "rivincita fra umani: due rivincite tornano al punto di partenza");
    }
}
