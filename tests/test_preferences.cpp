/**
 * @file test_preferences.cpp
 * @brief Application preferences: parsing, serialisation, files.
 *
 * The preferences file is untrusted input just as a save is, with one important
 * difference: a malformed save may be rejected, while an unreadable preference
 * must not block startup. Most of these checks verify precisely that a broken file
 * is ignored rather than refused.
 */

#include "core/app_preferences.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "test_framework.h"

using namespace hexapp;

namespace {

    /** @brief Temporary path used by the file checks, removed at the end. */
    std::string tempPath(const std::string& name) {
        return (std::filesystem::temp_directory_path() / name).string();
    }

    /** @brief Writes a fixture file with the given contents. */
    void writeFile(const std::string& path, const std::string& content) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << content;
    }
}

void preferences() {

    // --- Defaults: those of a first run ---
    {
        constexpr Preferences fresh;
        CHECK(!fresh.fullscreen, "preferenze: si parte in finestra");
        CHECK(!fresh.muted, "preferenze: si parte con l'audio attivo");
        CHECK(!fresh.colorblind, "preferenze: si parte senza simboli sulle pedine");
        CHECK(fresh.music_volume > 0 && fresh.music_volume <= 100,
              "preferenze: si parte con la musica a un volume udibile");
    }

    // --- Round trip: what is written is read back ---
    {
        const Preferences original{.fullscreen = true, .muted = true};
        const Preferences back = parsePreferences(serializePreferences(original));

        CHECK(back == original, "andata e ritorno: entrambe attive");

        const Preferences mixed{.fullscreen = true, .muted = false};
        CHECK(parsePreferences(serializePreferences(mixed)) == mixed,
              "andata e ritorno: una attiva e una no");

        constexpr Preferences none;
        CHECK(parsePreferences(serializePreferences(none)) == none,
              "andata e ritorno: nessuna attiva");

        // Accessibility travels like the other preferences: whoever needs it enables
        // it once and must find it again on restart, not on every match.
        constexpr Preferences cb{.colorblind = true};
        CHECK(parsePreferences(serializePreferences(cb)) == cb,
              "andata e ritorno: daltonismo attivo");

        constexpr Preferences quiet{.music_volume = 0};
        CHECK(parsePreferences(serializePreferences(quiet)) == quiet,
              "andata e ritorno: musica spenta");

        constexpr Preferences loud{.music_volume = 100};
        CHECK(parsePreferences(serializePreferences(loud)) == loud,
              "andata e ritorno: musica al massimo");

        // A file written before the option existed lacks the key, yielding the
        // disabled state, which is what that file displayed.
        CHECK(!parsePreferences("fullscreen=1\nmuted=0\n").colorblind,
              "compatibilita': un file senza la chiave lascia il daltonismo spento");
    }

    // --- The text produced is readable by eye ---
    {
        const std::string text = serializePreferences({.fullscreen = true, .muted = false});
        CHECK(text.find("fullscreen=1") != std::string::npos,
              "serializzazione: la chiave fullscreen c'e'");
        CHECK(text.find("muted=0") != std::string::npos,
              "serializzazione: la chiave muted c'e'");
        CHECK(text.find("colorblind=0") != std::string::npos,
              "serializzazione: la chiave colorblind c'e'");
        CHECK(text.find("music_volume=") != std::string::npos,
              "serializzazione: la chiave music_volume c'e'");
        CHECK(text.front() == '#', "serializzazione: si apre con un commento");
    }

    // --- Music volume ---
    // Unlike a counter, an out-of-range value is clamped rather than discarded: a
    // file asking for 500 wants the maximum, and the default would be a more
    // surprising answer than 100.
    {
        CHECK(parsePreferences("music_volume=0").music_volume == 0, "volume: zero e' ammesso");
        CHECK(parsePreferences("music_volume=70").music_volume == 70, "volume: un valore in scala passa");
        CHECK(parsePreferences("music_volume=100").music_volume == 100, "volume: cento e' ammesso");
        CHECK(parsePreferences("music_volume=500").music_volume == 100,
              "volume: oltre il massimo viene riportato a cento");
        CHECK(parsePreferences("music_volume=-5").music_volume == Preferences{}.music_volume,
              "volume: un valore negativo non e' un numero e lascia il predefinito");
        CHECK(parsePreferences("music_volume=alto").music_volume == Preferences{}.music_volume,
              "volume: un valore illeggibile lascia il predefinito");

        // A file written before the option existed lacks the key.
        CHECK(parsePreferences("muted=0\n").music_volume == Preferences{}.music_volume,
              "compatibilita': senza la chiave il volume resta quello predefinito");
    }

    // --- The spellings accepted for true and false ---
    {
        CHECK(parsePreferences("fullscreen=1").fullscreen, "vero: 1");
        CHECK(parsePreferences("fullscreen=true").fullscreen, "vero: true");
        CHECK(parsePreferences("fullscreen=on").fullscreen, "vero: on");
        CHECK(parsePreferences("fullscreen=YES").fullscreen, "vero: maiuscole ignorate");
        CHECK(parsePreferences("fullscreen=si").fullscreen, "vero: si");

        CHECK(!parsePreferences("fullscreen=0").fullscreen, "falso: 0");
        CHECK(!parsePreferences("fullscreen=false").fullscreen, "falso: false");
        CHECK(!parsePreferences("fullscreen=off").fullscreen, "falso: off");
        CHECK(!parsePreferences("fullscreen=NO").fullscreen, "falso: no");
    }

    // --- Whitespace, comments and sections do not interfere ---
    {
        const Preferences p = parsePreferences(
            "# un commento\n"
            "; anche questo\n"
            "\n"
            "[hex]\n"
            "   fullscreen   =   1   \n"
            "\tMUTED\t=\tTRUE\t\n");

        CHECK(p.fullscreen, "tolleranza: spazi attorno a chiave e valore");
        CHECK(p.muted, "tolleranza: chiave in maiuscolo");
    }
    {
        // A file written on Windows carries a CR that is not part of the value.
        const Preferences p = parsePreferences("fullscreen=1\r\nmuted=1\r\n");
        CHECK(p.fullscreen && p.muted, "tolleranza: fine riga di Windows");
    }

    // --- What cannot be understood keeps the default ---
    {
        const Preferences p = parsePreferences(
            "colore_preferito=blu\n"     // chiave sconosciuta: da una versione futura
            "fullscreen\n"               // senza separatore
            "=1\n"                       // senza chiave
            "muted=\n"                   // senza valore
            "fullscreen=forse\n");       // valore non riconoscibile

        CHECK(!p.fullscreen, "file rotto: fullscreen resta al predefinito");
        CHECK(!p.muted, "file rotto: muted resta al predefinito");
    }
    {
        // An oversized line means a corrupt file: it is skipped, not truncated.
        const std::string huge = "fullscreen=" + std::string(MAX_PREFERENCE_LINE + 10, '1');
        CHECK(!parsePreferences(huge).fullscreen, "file rotto: riga smisurata ignorata");
    }
    {
        // A valid preference survives the broken lines around it.
        const Preferences p = parsePreferences("spazzatura\nmuted=1\naltra spazzatura\n");
        CHECK(p.muted, "file rotto: le righe buone restano leggibili");
    }
    {
        CHECK(parsePreferences("") == Preferences{}, "testo vuoto: valori predefiniti");
    }
    {
        // The last line wins, as in every configuration file.
        CHECK(parsePreferences("muted=1\nmuted=0\n").muted == false,
              "chiave ripetuta: vince l'ultima");
    }

    // --- Files: writing, reading back, absence ---
    {
        const std::string path = tempPath("hex_test_prefs.ini");
        std::filesystem::remove(path);

        CHECK(loadPreferences(path) == Preferences{},
              "file assente: si ottengono i predefiniti");

        const Preferences saved{.fullscreen = true, .muted = true};
        CHECK(savePreferences(saved, path), "file: la scrittura riesce");
        CHECK(std::filesystem::is_regular_file(path), "file: il file esiste");
        CHECK(loadPreferences(path) == saved, "file: si rilegge quello che si e' scritto");

        // Overwriting must replace, not append.
        constexpr Preferences changed{.fullscreen = false, .muted = true};
        CHECK(savePreferences(changed, path), "file: la riscrittura riesce");
        CHECK(loadPreferences(path) == changed, "file: la riscrittura sostituisce");

        std::filesystem::remove(path);
    }
    {
        // An unreadable file must not block startup.
        const std::string path = tempPath("hex_test_prefs_rotto.ini");
        writeFile(path, "\x01\x02\x03 non sono preferenze\n");

        CHECK(loadPreferences(path) == Preferences{},
              "file illeggibile: si ottengono i predefiniti");

        std::filesystem::remove(path);
    }
    {
        // An impossible destination fails without throwing.
        CHECK(!savePreferences({}, tempPath("cartella_inesistente_hex/x/prefs.ini")),
              "file: una destinazione impossibile ritorna false");
    }
}
