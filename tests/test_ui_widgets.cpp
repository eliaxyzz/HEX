/**
 * @file test_ui_widgets.cpp
 * @brief Interface widgets and match settings.
 */

#include "core/game_settings.h"
#include "ui/resource_cache.h"
#include "ui/ui_widgets.h"

#include <memory>
#include <string>

#include "core/localization.h"
#include "test_framework.h"

using namespace hexui;
using namespace hexapp;

namespace {

    /** @brief Stub resource counting its own constructions. */
    struct FakeResource {
        std::string key;
        static inline int built = 0;

        explicit FakeResource(std::string k) : key(std::move(k)) { ++built; }
    };
}

/** @brief Default-language strings: the checks compare against that text. */
namespace { const hexui::LocalizationManager loc; }

void ui_widgets() {

    // --- Resource cache: load exactly once ---
    {
        FakeResource::built = 0;

        hexassets::ResourceCache<FakeResource> cache([](const std::string& key) {
            return std::make_unique<FakeResource>(key);
        });

        CHECK(cache.size() == 0, "cache: vuota alla creazione");
        CHECK(!cache.contains("font"), "cache: non contiene nulla");

        const FakeResource* first = cache.get("font");
        CHECK(first != nullptr, "cache: la prima richiesta costruisce la risorsa");
        CHECK(FakeResource::built == 1, "cache: una sola costruzione");
        CHECK(first->key == "font", "cache: la risorsa e' quella richiesta");

        const FakeResource* again = cache.get("font");
        CHECK(again == first, "cache: la seconda richiesta restituisce la stessa istanza");
        CHECK(FakeResource::built == 1, "cache: nessuna seconda costruzione");

        cache.get("texture");
        CHECK(FakeResource::built == 2, "cache: una chiave diversa viene costruita");
        CHECK(cache.size() == 2, "cache: due chiavi memorizzate");

        // A pointer handed out earlier must stay valid after the map has grown, which
        // is why the resources are held through unique_ptr.
        CHECK(first->key == "font", "cache: i riferimenti gia' consegnati restano validi");
    }
    {
        // A failed load is remembered: the missing file is not looked up again on
        // every frame.
        int attempts = 0;
        hexassets::ResourceCache<FakeResource> cache([&attempts](const std::string&) {
            ++attempts;
            return std::unique_ptr<FakeResource>{};
        });

        CHECK(cache.get("assente") == nullptr, "cache: risorsa non disponibile");
        CHECK(cache.get("assente") == nullptr, "cache: ancora non disponibile");
        CHECK(attempts == 1, "cache: il caricamento fallito non viene ritentato");
        CHECK(cache.contains("assente"), "cache: anche il fallimento e' memorizzato");
    }

    // --- Second player ---
    {
        GameSettings s;
        s.mode = GameMode::HUMAN_VS_HUMAN;
        s.player_name = "Elia";
        s.opponent_name = "Marco";

        CHECK(firstPlayerName(s, loc) == "Elia", "due giocatori: il primo nome");
        CHECK(secondPlayerName(s, loc) == "Marco", "due giocatori: il secondo nome");

        s.opponent_name.clear();
        CHECK(secondPlayerName(s, loc) == "Umano 2", "due giocatori: senza nome, un ripiego");

        s.player_name.clear();
        CHECK(firstPlayerName(s, loc) == "Umano", "due giocatori: ripiego anche per il primo");
        CHECK(firstPlayerName(s, loc) != secondPlayerName(s, loc),
              "due giocatori: i ripieghi restano distinguibili");
    }
    {
        // Against the engine the second player's name has no owner; the level is what
        // matters.
        GameSettings s;
        s.mode = GameMode::HUMAN_VS_AI;
        s.difficulty = Difficulty::EASY;
        s.player_name = "Elia";

        CHECK(secondPlayerName(s, loc) == "Computer Facile",
              "contro il Computer: l'avversario e' il livello");

        // If a real name is present, from a save for instance, it wins.
        s.opponent_name = "IA Difficile";
        CHECK(secondPlayerName(s, loc) == "IA Difficile",
              "contro l'IA: un nome esplicito ha la precedenza");
    }

    // --- Rect ---
    {
        constexpr Rect r{10.0f, 20.0f, 100.0f, 40.0f};
        CHECK(r.contains(10.0f, 20.0f), "rect: l'angolo superiore sinistro e' dentro");
        CHECK(r.contains(110.0f, 60.0f), "rect: l'angolo inferiore destro e' dentro");
        CHECK(r.contains(60.0f, 40.0f), "rect: il centro e' dentro");
        CHECK(!r.contains(9.9f, 40.0f), "rect: appena a sinistra e' fuori");
        CHECK(!r.contains(60.0f, 60.1f), "rect: appena sotto e' fuori");
        CHECK(!Rect{}.contains(0.0f, 0.0f) == false, "rect: un rettangolo nullo contiene la sua origine");
    }

    // --- Button ---
    {
        Button b("GIOCA", {0.0f, 0.0f, 100.0f, 40.0f});
        CHECK(b.label() == "GIOCA", "bottone: etichetta");
        CHECK(b.enabled(), "bottone: abilitato di default");
        CHECK(b.state() == WidgetState::NORMAL, "bottone: a riposo");

        b.onMouseMove(50.0f, 20.0f);
        CHECK(b.state() == WidgetState::HOVERED, "bottone: sotto il puntatore");

        b.onMouseMove(500.0f, 20.0f);
        CHECK(b.state() == WidgetState::NORMAL, "bottone: il puntatore se ne va");

        CHECK(b.onMousePress(50.0f, 20.0f), "bottone: il click dentro lo attiva");
        CHECK(b.state() == WidgetState::PRESSED, "bottone: premuto");
        CHECK(!b.onMousePress(500.0f, 20.0f), "bottone: il click fuori non lo attiva");
    }
    {
        // A disabled button does not react, and says so to the drawing code.
        Button b("Annulla", {0.0f, 0.0f, 100.0f, 40.0f});
        b.setEnabled(false);
        CHECK(b.state() == WidgetState::DISABLED, "bottone: disabilitato");
        b.onMouseMove(50.0f, 20.0f);
        CHECK(b.state() == WidgetState::DISABLED, "bottone: disabilitato ignora il puntatore");
        CHECK(!b.onMousePress(50.0f, 20.0f), "bottone: disabilitato ignora il click");

        b.setEnabled(true);
        CHECK(b.state() == WidgetState::NORMAL, "bottone: riabilitato torna a riposo");
        CHECK(b.onMousePress(50.0f, 20.0f), "bottone: riabilitato risponde di nuovo");
    }

    // --- TextInput ---
    {
        TextInput input("Il tuo nome", {0.0f, 0.0f, 200.0f, 40.0f}, 5);
        CHECK(input.text().empty(), "campo: vuoto alla creazione");
        CHECK(input.placeholder() == "Il tuo nome", "campo: segnaposto");
        CHECK(!input.focused(), "campo: senza fuoco alla creazione");

        // Without focus no character enters, which is what stops a key pressed
        // elsewhere from landing in the nickname.
        CHECK(!input.onCharacter(U'a'), "campo: senza fuoco non accetta caratteri");
        CHECK(input.text().empty(), "campo: resta vuoto");

        CHECK(input.onMousePress(50.0f, 20.0f), "campo: il click dentro da' il fuoco");
        CHECK(input.focused(), "campo: ha il fuoco");

        CHECK(input.onCharacter(U'E'), "campo: accetta un carattere");
        CHECK(input.onCharacter(U'l'), "campo: accetta il secondo");
        CHECK(input.onCharacter(U'i'), "campo: accetta il terzo");
        CHECK(input.text() == "Eli", "campo: il testo si compone");
        CHECK(input.length() == 3, "campo: tre caratteri");

        CHECK(input.onBackspace(), "campo: cancella");
        CHECK(input.text() == "El", "campo: l'ultimo carattere sparisce");

        CHECK(!input.onMousePress(500.0f, 20.0f), "campo: il click fuori non lo colpisce");
        CHECK(!input.focused(), "campo: il click fuori toglie il fuoco");
        CHECK(input.text() == "El", "campo: perdere il fuoco non cancella il testo");
    }
    {
        // Control characters are never accepted; otherwise a newline would end up in
        // a name and break the save format.
        TextInput input("nome", {0.0f, 0.0f, 200.0f, 40.0f}, 16);
        input.setFocused(true);

        CHECK(!input.onCharacter(U'\n'), "campo: rifiuta l'a capo");
        CHECK(!input.onCharacter(U'\t'), "campo: rifiuta la tabulazione");
        CHECK(!input.onCharacter(U'\r'), "campo: rifiuta il ritorno a capo");
        CHECK(!input.onCharacter(static_cast<char32_t>(8)), "campo: rifiuta il backspace come carattere");
        CHECK(!input.onCharacter(static_cast<char32_t>(0x7F)), "campo: rifiuta il carattere cancella");
        CHECK(input.text().empty(), "campo: nessun carattere di controllo e' entrato");

        CHECK(input.onCharacter(U' '), "campo: lo spazio e' un carattere valido");
        CHECK(input.text() == " ", "campo: lo spazio entra");
    }
    {
        // The limit is enforced before insertion.
        TextInput input("nome", {0.0f, 0.0f, 200.0f, 40.0f}, 4);
        input.setFocused(true);
        for (const char32_t c : std::u32string(U"abcdefgh")) input.onCharacter(c);

        CHECK(input.length() == 4, "campo: il limite di lunghezza e' rispettato");
        CHECK(input.text() == "abcd", "campo: i caratteri in eccesso sono scartati");
        CHECK(!input.onCharacter(U'z'), "campo: pieno, rifiuta");
        CHECK(input.onBackspace() && input.onCharacter(U'z'), "campo: liberato un posto, accetta di nuovo");
        CHECK(input.text() == "abcz", "campo: il nuovo carattere prende il posto liberato");
    }
    {
        // Accents: a non-ASCII character takes one slot, and deletion removes the
        // whole character rather than one byte.
        TextInput input("nome", {0.0f, 0.0f, 200.0f, 40.0f}, 8);
        input.setFocused(true);
        input.onCharacter(U'E');
        input.onCharacter(U'l');
        input.onCharacter(U'i');
        input.onCharacter(U'à');

        CHECK(input.length() == 4, "campo: un accento conta come un carattere");
        CHECK(input.text().size() == 5, "campo: in UTF-8 occupa due byte");
        CHECK(input.onBackspace(), "campo: cancella l'accento");
        CHECK(input.text() == "Eli", "campo: cancellato il carattere intero, non mezzo byte");
        CHECK(input.length() == 3, "campo: tre caratteri rimasti");
    }
    {
        // Prefilling: what renaming a save needs, where the field starts from the
        // name the file already has.
        TextInput input("nome", {0.0f, 0.0f, 200.0f, 40.0f}, 8);

        input.setText("Elia");
        CHECK(input.text() == "Elia", "campo: il testo precompilato si rilegge");
        CHECK(input.length() == 4, "campo: e conta i caratteri che ha");

        // Replaces rather than appends: a second setText leaves nothing of the first.
        input.setText("Bo");
        CHECK(input.text() == "Bo", "campo: il testo precompilato sostituisce il precedente");
        CHECK(input.length() == 2, "campo: e la lunghezza lo segue");

        // The text goes through the same filters as typing: the capacity applies here
        // too, or an over-long name would slip in through the back door.
        input.setText("dodici caratteri e passa");
        CHECK(input.length() == 8, "campo: la precompilazione rispetta la capacita'");

        // An accent stays a single character even when arriving as ready-made UTF-8.
        input.setText("Elià");
        CHECK(input.length() == 4, "campo: l'accento precompilato conta uno");
        CHECK(input.text() == "Elià", "campo: e si ricodifica identico");

        // Control characters are discarded by onCharacter, as they are for the keyboard.
        input.setText("a\nb");
        CHECK(input.text() == "ab", "campo: la precompilazione scarta i caratteri di controllo");
    }
    {
        // Blinking caret.
        TextInput input("nome", {0.0f, 0.0f, 200.0f, 40.0f});
        CHECK(!input.caretVisible(), "campo: nessun cursore senza fuoco");

        input.setFocused(true);
        CHECK(input.caretVisible(), "campo: preso il fuoco, il cursore si vede subito");

        input.update(0.6f);
        CHECK(!input.caretVisible(), "campo: dopo mezzo periodo il cursore sparisce");
        input.update(0.6f);
        CHECK(input.caretVisible(), "campo: e poi riappare");

        input.setFocused(false);
        input.update(0.6f);
        CHECK(!input.caretVisible(), "campo: senza fuoco il cursore resta spento");
    }

    // --- OptionGroup ---
    {
        OptionGroup group({"Facile", "Medio", "Difficile"}, {0.0f, 0.0f, 300.0f, 40.0f}, 1);
        CHECK(group.options().size() == 3, "opzioni: tre voci");
        CHECK(group.selected() == 1, "opzioni: scelta iniziale");

        // The entries divide the bounds into equal parts.
        CHECK(group.optionBounds(0).x == 0.0f && group.optionBounds(0).w == 100.0f,
              "opzioni: la prima occupa il primo terzo");
        CHECK(group.optionBounds(2).x == 200.0f, "opzioni: la terza inizia a due terzi");

        CHECK(group.onMousePress(250.0f, 20.0f), "opzioni: il click cambia la scelta");
        CHECK(group.selected() == 2, "opzioni: scelta la terza");

        CHECK(!group.onMousePress(250.0f, 20.0f), "opzioni: ricliccare la stessa non cambia nulla");
        CHECK(!group.onMousePress(250.0f, 500.0f), "opzioni: il click fuori non cambia nulla");
        CHECK(group.selected() == 2, "opzioni: la scelta resta");

        group.onMouseMove(50.0f, 20.0f);
        CHECK(group.hovered() == 0, "opzioni: la prima e' sotto il puntatore");
        group.onMouseMove(50.0f, 500.0f);
        CHECK(group.hovered() == -1, "opzioni: nessuna sotto il puntatore");

        group.setSelected(9);
        CHECK(group.selected() == 2, "opzioni: un indice fuori intervallo viene ignorato");
        group.setSelected(0);
        CHECK(group.selected() == 0, "opzioni: un indice valido viene accettato");
    }

    // --- OptionGroup: locked entries, i.e. rewards not yet unlocked ---
    {
        OptionGroup group({"Classico", "Tossico", "Prestigio"},
                          {0.0f, 0.0f, 300.0f, 40.0f}, 0);

        quiet_failures = 0;
        for (int i = 0; i < 3; ++i) CHECK_QUIET(group.optionEnabled(i));
        CHECK(quiet_failures == 0, "lucchetti: alla costruzione sono tutte disponibili");

        group.setOptionEnabled(2, false);
        CHECK(!group.optionEnabled(2), "lucchetti: la terza risulta bloccata");
        CHECK(group.optionEnabled(1), "lucchetti: le altre non vengono toccate");

        // A click passes over them: the check that stops anything the drawing shows
        // only in grey from being selected.
        CHECK(!group.onMousePress(250.0f, 20.0f), "lucchetti: il click su una bloccata non sceglie");
        CHECK(group.selected() == 0, "lucchetti: la scelta resta dov'era");

        // Nor does the pointer light one up: no promise made before the click.
        group.onMouseMove(250.0f, 20.0f);
        CHECK(group.hovered() == -1, "lucchetti: una bloccata non si illumina");

        // Programmatic selection honours the lock too: a preference read from a file
        // must not activate what a click cannot.
        group.setSelected(2);
        CHECK(group.selected() == 0, "lucchetti: setSelected rifiuta una voce bloccata");

        // An unlocked entry becomes selectable again in every respect.
        group.setOptionEnabled(2, true);
        CHECK(group.onMousePress(250.0f, 20.0f), "lucchetti: sbloccata, il click funziona");
        CHECK(group.selected() == 2, "lucchetti: ed e' diventata la scelta");

        // Locking the entry that is currently selected cannot leave it selected: that
        // would be a choice impossible to leave.
        group.setOptionEnabled(2, false);
        CHECK(group.selected() != 2, "lucchetti: bloccare la scelta corrente la fa ripiegare");
        CHECK(group.optionEnabled(group.selected()),
              "lucchetti: e il ripiego cade su una voce disponibile");

        CHECK(!group.optionEnabled(9), "lucchetti: un indice fuori intervallo non e' disponibile");
    }

    // --- Settings and difficulty ---
    {
        CHECK(configFor(Difficulty::EASY).time_limit_ms == 100.0, "difficolta': Facile pensa 100 ms");
        CHECK(!configFor(Difficulty::EASY).save_bridges, "difficolta': Facile senza difesa dei ponti");

        CHECK(configFor(Difficulty::MEDIUM).time_limit_ms == 500.0, "difficolta': Medio pensa 500 ms");
        CHECK(!configFor(Difficulty::MEDIUM).save_bridges, "difficolta': Medio senza difesa dei ponti");

        CHECK(configFor(Difficulty::HARD).time_limit_ms == 2000.0, "difficolta': Difficile pensa 2000 ms");
        CHECK(configFor(Difficulty::HARD).save_bridges, "difficolta': Difficile difende i ponti");

        // The scale must be monotonic in time: a level cannot think less than the one
        // below it.
        CHECK(configFor(Difficulty::EASY).time_limit_ms < configFor(Difficulty::MEDIUM).time_limit_ms
              && configFor(Difficulty::MEDIUM).time_limit_ms < configFor(Difficulty::HARD).time_limit_ms,
              "difficolta': il tempo di riflessione cresce con il livello");
    }
    {
        GameSettings s;
        s.player_name = "";
        CHECK(firstPlayerName(s, loc) == "Umano", "impostazioni: nickname vuoto ha un ripiego");

        s.player_name = "Elia";
        CHECK(firstPlayerName(s, loc) == "Elia", "impostazioni: il nickname scelto viene usato");

        s.mode = GameMode::HUMAN_VS_AI;
        s.difficulty = Difficulty::HARD;
        CHECK(secondPlayerName(s, loc) == "Computer Difficile",
              "impostazioni: l'avversario e' il Computer col suo livello");

        s.mode = GameMode::HUMAN_VS_HUMAN;
        CHECK(secondPlayerName(s, loc) == "Umano 2", "impostazioni: in due il secondo e' un umano");

        CHECK(loc.text(labelOf(GameMode::HUMAN_VS_AI)) == "Umano vs Computer", "impostazioni: etichetta della modalita'");
        CHECK(loc.text(labelOf(Difficulty::MEDIUM)) == "Medio", "impostazioni: etichetta del livello");
    }
}
