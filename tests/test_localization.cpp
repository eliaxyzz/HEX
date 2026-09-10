/**
 * @file test_localization.cpp
 * @brief Translation coverage and language switching.
 *
 * The first check is the one that matters: every key has a phrase in both
 * languages. A forgotten translation does not show while playing, since the screen
 * missing it may not be the one that opens, and it does not show at compile time
 * either. It shows here, by walking the whole table.
 */

#include "core/localization.h"

#include <set>
#include <string>

#include "core/app_preferences.h"
#include "core/game_settings.h"
#include "test_framework.h"

using namespace hexui;

void localization() {

    // --- Every key has a phrase, in both languages ---
    {
        quiet_failures = 0;
        for (std::size_t i = 0; i < STRING_COUNT; ++i) {
            const auto key = static_cast<StringKey>(i);
            CHECK_QUIET(!LocalizationManager::textIn(Language::IT, key).empty());
            CHECK_QUIET(!LocalizationManager::textIn(Language::EN, key).empty());
        }
        CHECK(quiet_failures == 0, "copertura: tutte le chiavi hanno italiano e inglese");
        CHECK(STRING_COUNT > 60, "copertura: la tabella contiene tutte le frasi previste");
    }
    {
        // No phrase is made only of spaces, which would be empty in practice.
        quiet_failures = 0;
        for (std::size_t i = 0; i < STRING_COUNT; ++i) {
            const auto key = static_cast<StringKey>(i);
            for (const Language lang : {Language::IT, Language::EN}) {
                const std::string& text = LocalizationManager::textIn(lang, key);
                CHECK_QUIET(text.find_first_not_of(" \t") != std::string::npos);
            }
        }
        CHECK(quiet_failures == 0, "copertura: nessuna frase e' fatta di soli spazi");
    }
    {
        // A label identical across the two languages is nearly always a forgotten
        // translation. The exceptions are few and declared, so adding one is a
        // decision rather than an oversight.
        const std::set<StringKey> same_on_purpose{
            StringKey::GAME_PIE_RULE,          // termine tecnico, uguale in inglese
            StringKey::SETTINGS_LANGUAGE_IT,   // il nome di una lingua non si traduce
            StringKey::SETTINGS_LANGUAGE_EN,
            StringKey::MENU_TUTORIAL,          // parola d'uso internazionale, non si traduce
            StringKey::NAME_COMPUTER,          // il nome dell'avversario non umano, uguale ovunque
            StringKey::MENU_VARIANT_ARCADE,    // nome della modalita', non si traduce
            StringKey::RANK_HACKER,            // titoli d'uso internazionale: restano
            StringKey::RANK_MASTERMIND,        // uguali in entrambe le lingue
        };

        quiet_failures = 0;
        for (std::size_t i = 0; i < STRING_COUNT; ++i) {
            const auto key = static_cast<StringKey>(i);
            if (same_on_purpose.contains(key)) continue;

            CHECK_QUIET(LocalizationManager::textIn(Language::IT, key)
                        != LocalizationManager::textIn(Language::EN, key));
        }
        CHECK(quiet_failures == 0, "copertura: le due lingue differiscono davvero");
    }

    // --- Language switching ---
    {
        LocalizationManager loc;
        CHECK(loc.language() == Language::IT, "lingua: l'italiano e' il predefinito");
        CHECK(loc.text(StringKey::MENU_PLAY) == "GIOCA", "lingua: la frase e' in italiano");

        loc.setLanguage(Language::EN);
        CHECK(loc.language() == Language::EN, "lingua: cambiata in inglese");
        CHECK(loc.text(StringKey::MENU_PLAY) == "PLAY", "lingua: la frase e' cambiata subito");

        loc.setLanguage(Language::IT);
        CHECK(loc.text(StringKey::MENU_PLAY) == "GIOCA", "lingua: si torna indietro");
    }
    {
        const LocalizationManager english(Language::EN);
        CHECK(english.text(StringKey::MENU_QUIT) == "Quit", "lingua: costruito in inglese");
    }
    {
        // A reference handed out stays valid across a language change: both tables
        // exist at once and neither overwrites the other.
        LocalizationManager loc(Language::IT);
        const std::string& italian = loc.text(StringKey::MENU_SETTINGS);
        loc.setLanguage(Language::EN);
        CHECK(italian == "Impostazioni", "lingua: i riferimenti gia' consegnati restano validi");
    }

    // --- Language codes, the ones that reach the file ---
    {
        CHECK(LocalizationManager::codeOf(Language::IT) == "it", "codice: italiano");
        CHECK(LocalizationManager::codeOf(Language::EN) == "en", "codice: inglese");

        CHECK(LocalizationManager::languageFromCode("it") == Language::IT, "codice: 'it' riconosciuto");
        CHECK(LocalizationManager::languageFromCode("en") == Language::EN, "codice: 'en' riconosciuto");
        CHECK(LocalizationManager::languageFromCode("fr") == std::nullopt,
              "codice: una lingua che non c'e' viene rifiutata");
        CHECK(LocalizationManager::languageFromCode("") == std::nullopt, "codice: vuoto rifiutato");
        CHECK(LocalizationManager::languageFromCode("IT") == std::nullopt,
              "codice: il confronto e' esatto, la normalizzazione spetta a chi legge il file");
    }

    // --- Persistence in the preferences ---
    {
        const hexapp::Preferences saved{.language = Language::EN};
        const std::string text = hexapp::serializePreferences(saved);
        CHECK(text.find("language=en") != std::string::npos, "preferenze: la lingua viene scritta");

        CHECK(hexapp::parsePreferences(text).language == Language::EN,
              "preferenze: la lingua si rilegge");
    }
    {
        // Backward compatibility: a file written before the language key existed
        // lacks it and must keep showing the default language.
        const hexapp::Preferences old = hexapp::parsePreferences(
            "fullscreen=1\n"
            "muted=0\n"
            "wins=3\n"
            "losses=2\n");

        CHECK(old.language == Language::IT, "vecchio file: senza la chiave si usa l'italiano");
        CHECK(old.fullscreen && old.wins == 3, "vecchio file: il resto viene letto normalmente");
    }
    {
        // An unknown code does not reject the file: it falls back to the default.
        CHECK(hexapp::parsePreferences("language=klingon\n").language == Language::IT,
              "codice ignoto: si ripiega sull'italiano");
        CHECK(hexapp::parsePreferences("language=\n").language == Language::IT,
              "codice vuoto: si ripiega sull'italiano");

        // The file is written lowercase, but a hand edit may not be: reading
        // normalises the case before comparing.
        CHECK(hexapp::parsePreferences("language=EN\n").language == Language::EN,
              "codice in maiuscolo: riconosciuto lo stesso");
    }

    // --- Match settings speak in keys, not in phrases ---
    {
        const LocalizationManager italian(Language::IT);
        const LocalizationManager english(Language::EN);

        hexapp::GameSettings s;
        s.mode = hexapp::GameMode::HUMAN_VS_AI;
        s.difficulty = hexapp::Difficulty::HARD;

        CHECK(italian.text(labelOf(s.mode)) == "Umano vs Computer", "impostazioni: modalita' in italiano");
        CHECK(english.text(labelOf(s.mode)) == "Human vs Computer", "impostazioni: modalita' in inglese");
        CHECK(italian.text(labelOf(s.difficulty)) == "Difficile", "impostazioni: livello in italiano");
        CHECK(english.text(labelOf(s.difficulty)) == "Hard", "impostazioni: livello in inglese");

        // The fallback names follow the language too.
        CHECK(hexapp::nameOfColour(s, hex::Player::BLUE, italian) == "Computer Difficile",
              "impostazioni: il nome del Computer in italiano");
        CHECK(hexapp::nameOfColour(s, hex::Player::BLUE, english) == "Computer Hard",
              "impostazioni: il nome del Computer in inglese");
        CHECK(hexapp::nameOfColour(s, hex::Player::RED, english) == "Human",
              "impostazioni: il nome di ripiego dell'umano in inglese");
    }
}
