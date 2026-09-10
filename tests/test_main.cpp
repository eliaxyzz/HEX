/**
 * @file test_main.cpp
 * @brief Suite entry point.
 *
 * With no arguments it runs every group. Given a group name it runs only that
 * one, which is how CTest registers a separate test per area so that a failure
 * names the part of the system that broke.
 */

#include "test_framework.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

void rules();
void console_view();
void controller();
void async_player();
void engine();
void history();
void geometry();
void hit_test();
void gui_input();
void gui_observer();
void gui_loop();
void gui_ai();
void arbiter();
void app_states();
void ui_widgets();
void preferences();
// Plain `clock` would collide with ::clock from <ctime>.
void game_clock();
void localization();
void network();
void save_format();
void winning_path();
void arcade();
void profile();

namespace {

    /** @brief One assertion group registered in the suite. */
    struct Group {
        std::string_view name;
        void (*run)();
        std::string_view description;
    };

    const std::vector<Group> GROUPS = {
    {"rules", rules, "Regole, stato immutabile ed esiti della partita"},
    {"console_view", console_view, "Rendering testuale e formattazione"},
    {"controller", controller, "GameController a passi e notifiche agli observer"},
    {"async_player", async_player, "Contratto asincrono di AbstractPlayer"},
    {"engine", engine, "Motore MCTS: cancellazione, dimensione, tempo"},
    {"history", history, "Cronologia delle mosse e annullamento"},
    {"geometry", geometry, "Geometria della griglia esagonale"},
    {"hit_test", hit_test, "Conversione da pixel a cella"},
    {"gui_input", gui_input, "SfmlHumanPlayer: click, validazione, Pie Rule"},
    {"gui_observer", gui_observer, "SfmlGameObserver: stato visivo"},
    {"gui_loop", gui_loop, "Cablaggio del loop grafico, simulato"},
    {"gui_ai", gui_ai, "Partita umano contro MCTS"},
    {"arbiter", arbiter, "HexGameRuler: timeout, resa, partita completa"},
    {"app_states", app_states, "Macchina a stati dell'applicazione"},
    {"ui_widgets", ui_widgets, "Widget dell'interfaccia e impostazioni"},
    {"preferences", preferences, "Preferenze dell'applicazione e file settings.ini"},
    {"clock", game_clock, "Orologio di partita e ordine dei turni"},
    {"localization", localization, "Traduzioni IT/EN e copertura delle chiavi"},
    {"network", network, "Protocollo di rete: pacchetti e rifiuti"},
    {"save_format", save_format, "Formato di salvataggio e riproduzione"},
    {"winning_path", winning_path, "Catena vincente e curve di animazione"},
    {"arcade", arcade, "Modalita' Arcade: buchi neri e orologio Blitz"},
    {"profile", profile, "Profilo del giocatore, XP e salvataggi Arcade"},
    };
}

int main(const int argc, char** argv) {
    std::cout << std::unitbuf;

    const std::string_view requested = (argc > 1) ? argv[1] : "";

    if (requested == "--list") {
        for (const Group& g : GROUPS) std::cout << g.name << " - " << g.description << "\n";
        return 0;
    }

    int executed = 0;
    for (const Group& g : GROUPS) {
        if (!requested.empty() && g.name != requested) continue;
        std::cout << "\n=== " << g.name << " - " << g.description << " ===\n";
        g.run();
        ++executed;
    }

    if (executed == 0) {
        std::cout << "Gruppo sconosciuto: " << requested << "\nGruppi disponibili:\n";
        for (const Group& g : GROUPS) std::cout << "  " << g.name << "\n";
        return 2;
    }

    std::cout << (failures ? "\nFAILURES\n" : "\nALL PASS\n");
    return failures != 0;
}
