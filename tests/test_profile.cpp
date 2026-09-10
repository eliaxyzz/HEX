/**
 * @file test_profile.cpp
 * @brief Player progression and the Arcade save round trip.
 *
 * Two areas that barely touch but share one question: what survives shutting the
 * game down, and does it survive intact?
 */

#include "core/player_profile.h"

#include <filesystem>
#include <string>

#include "core/app_preferences.h"
#include "core/localization.h"
#include "core/save_format.h"
#include "test_framework.h"

using namespace hexapp;

void profile() {

    // --- Levels ---
    {
        CHECK(levelFor(0) == 1, "livello: chi non ha giocato e' di livello 1, non 0");
        CHECK(levelFor(499) == 1, "livello: 499 XP e' ancora il primo livello");
        CHECK(levelFor(500) == 2, "livello: a 500 XP si sale al secondo");
        CHECK(levelFor(1250) == 3, "livello: 1250 XP e' il terzo livello");
        CHECK(levelFor(-100) == 1, "livello: un valore negativo vale come nessuna esperienza");

        CHECK(xpIntoLevel(0) == 0, "progresso: si parte da zero dentro il livello");
        CHECK(xpIntoLevel(1250) == 250, "progresso: 1250 XP sono 250 dentro il terzo livello");
        CHECK(xpIntoLevel(500) == 0, "progresso: appena saliti si riparte da zero");
    }

    // --- Rank titles ---
    {
        CHECK(titleKeyFor(1) == hexui::StringKey::RANK_ROOKIE, "titolo: il livello 1 e' Novellino");
        CHECK(titleKeyFor(2) == hexui::StringKey::RANK_HACKER, "titolo: il livello 2 e' Hacker");
        CHECK(titleKeyFor(3) == hexui::StringKey::RANK_MASTERMIND, "titolo: il livello 3 e' Mastermind");
        CHECK(titleKeyFor(4) == hexui::StringKey::RANK_LEGEND, "titolo: dal livello 4 si e' Leggenda");
        CHECK(titleKeyFor(99) == hexui::StringKey::RANK_LEGEND, "titolo: e da li' in poi non cambia");

        // The title follows the level, which follows the experience: the full chain.
        const hexui::LocalizationManager it(hexui::Language::IT);
        CHECK(it.text(titleKeyFor(levelFor(0))) == "Novellino", "titolo: a zero XP si e' Novellino");
        CHECK(it.text(titleKeyFor(levelFor(600))) == "Hacker", "titolo: a 600 XP si e' Hacker");
        CHECK(it.text(titleKeyFor(levelFor(1250))) == "Mastermind",
              "titolo: a 1250 XP si e' Mastermind");
    }

    // --- Palette unlocking ---
    {
        CHECK(requiredLevelFor(BoardPalette::CLASSIC) == 1, "sblocco: il classico c'e' da subito");
        CHECK(requiredLevelFor(BoardPalette::TOXIC) == 2, "sblocco: il tossico dal livello 2");
        CHECK(requiredLevelFor(BoardPalette::PRESTIGE) == 3, "sblocco: il prestigio dal livello 3");

        CHECK(isPaletteUnlocked(BoardPalette::CLASSIC, 0), "sblocco: il classico non si sblocca");
        CHECK(!isPaletteUnlocked(BoardPalette::TOXIC, 499), "sblocco: a 499 XP il tossico e' chiuso");
        CHECK(isPaletteUnlocked(BoardPalette::TOXIC, 500), "sblocco: a 500 XP si apre");
        CHECK(!isPaletteUnlocked(BoardPalette::PRESTIGE, 999), "sblocco: a 999 XP il prestigio e' chiuso");
        CHECK(isPaletteUnlocked(BoardPalette::PRESTIGE, 1000), "sblocco: a 1000 XP si apre");
    }

    // --- A choice that is no longer legitimate falls back ---
    {
        // The case of a hand-edited settings.ini, or of a profile reset after an
        // advanced palette was chosen.
        CHECK(usablePalette(BoardPalette::PRESTIGE, 0) == BoardPalette::CLASSIC,
              "ripiego: una tavolozza non sbloccata non viene onorata");
        CHECK(usablePalette(BoardPalette::TOXIC, 0) == BoardPalette::CLASSIC,
              "ripiego: vale anche per il tossico");
        CHECK(usablePalette(BoardPalette::PRESTIGE, 1500) == BoardPalette::PRESTIGE,
              "ripiego: una scelta legittima passa intatta");
        CHECK(usablePalette(BoardPalette::CLASSIC, 0) == BoardPalette::CLASSIC,
              "ripiego: il classico vale sempre");
    }

    // --- The palette is remembered across runs ---
    {
        Preferences prefs;
        CHECK(prefs.palette == BoardPalette::CLASSIC, "preferenze: si parte dal classico");

        prefs.palette = BoardPalette::PRESTIGE;
        const std::string text = serializePreferences(prefs);
        CHECK(text.find("palette=prestige") != std::string::npos,
              "preferenze: la tavolozza viene scritta per nome");

        CHECK(parsePreferences(text).palette == BoardPalette::PRESTIGE,
              "preferenze: e si rilegge identica");

        CHECK(parsePreferences("palette=toxic").palette == BoardPalette::TOXIC,
              "preferenze: il tossico si rilegge");
        CHECK(parsePreferences("palette=viola").palette == BoardPalette::CLASSIC,
              "preferenze: un nome sconosciuto lascia il classico");

        // A file from an earlier version lacks the line, so classic stands.
        CHECK(parsePreferences("fullscreen=1\nmuted=0\n").palette == BoardPalette::CLASSIC,
              "preferenze: un file senza la riga della tavolozza resta al classico");
    }

    // --- Accumulation ---
    {
        const Profile fresh;
        CHECK(fresh.xp == 0, "profilo: un profilo nuovo parte da zero");

        const Profile after_win = withXpGain(fresh, XP_FOR_WIN);
        CHECK(after_win.xp == 100, "esperienza: una vittoria vale 100");

        const Profile after_loss = withXpGain(after_win, XP_FOR_LOSS);
        CHECK(after_loss.xp == 125, "esperienza: una sconfitta ne aggiunge 25");

        // The starting profile is left untouched: the caller receives a new value and
        // there is no way to lose the original by accident.
        CHECK(fresh.xp == 0, "esperienza: il profilo di partenza resta immutato");

        CHECK(withXpGain(after_loss, -1000).xp == 125,
              "esperienza: un guadagno negativo non toglie niente");

        // Exactly five wins reach level two: the number of matches a player watches
        // go by before ranking up.
        Profile p;
        for (int i = 0; i < 5; ++i) p = withXpGain(p, XP_FOR_WIN);
        CHECK(p.xp == 500 && levelFor(p.xp) == 2, "esperienza: cinque vittorie fanno il livello 2");
    }

    // --- Ceiling ---
    {
        const Profile full{MAX_XP};
        CHECK(withXpGain(full, XP_FOR_WIN).xp == MAX_XP, "tetto: l'esperienza non lo supera");
    }

    // --- Round trip through text ---
    {
        const Profile original{1250};
        const Profile back = parseProfile(serializeProfile(original));
        CHECK(back == original, "file: il profilo si rilegge identico");
    }

    // --- The file is untrusted input ---
    {
        CHECK(parseProfile("").xp == 0, "file: un file vuoto da' un profilo nuovo");
        CHECK(parseProfile("xp=abc").xp == 0, "file: un valore non numerico lascia il predefinito");
        CHECK(parseProfile("xp=-500").xp == 0, "file: un valore negativo viene rifiutato");
        CHECK(parseProfile("# solo commenti\n").xp == 0, "file: nessuna chiave, nessun dato");
        CHECK(parseProfile("livello=9\nxp=300").xp == 300,
              "file: una chiave sconosciuta viene ignorata, le altre si leggono");
        CHECK(parseProfile("XP = 700").xp == 700, "file: chiave in maiuscolo e spazi accettati");
        CHECK(parseProfile("xp=999999999999").xp == MAX_XP,
              "file: un valore smisurato viene riportato al tetto");
    }

    // --- On disk ---
    {
        const std::string path = "saves/profile_test_qa.ini";
        const Profile original{775};

        CHECK(saveProfile(original, path), "disco: il profilo viene scritto");
        CHECK(loadProfile(path) == original, "disco: e si rilegge identico");

        std::error_code ec;
        std::filesystem::remove(path, ec);

        CHECK(loadProfile(path).xp == 0, "disco: un file assente da' un profilo nuovo");
    }

    // --- Arcade saves: the black holes survive the round trip ---
    {
        hexsave::SaveData data;
        data.board_size = 11;
        data.mode = GameMode::HUMAN_VS_AI;
        data.red_name = "Elia";
        data.blue_name = "Computer Medio";
        data.holes = {{0, 1}, {0, 9}, {8, 8}, {10, 3}};

        const std::string text = hexsave::serialize(data);
        CHECK(text.find("hexsave 3") != std::string::npos,
              "arcade: un file con buchi neri dichiara la versione 3");
        CHECK(text.find("holes B1 J1 I9 D11") != std::string::npos,
              "arcade: i buchi sono scritti nella notazione delle celle");

        std::string error;
        const std::optional<hexsave::SaveData> back = hexsave::parse(text, error);
        CHECK(back.has_value(), "arcade: il file si rilegge");
        CHECK(back && back->holes == data.holes, "arcade: i buchi tornano identici e in ordine");

        // The mode is inferred from the holes, which is what switches Blitz back on.
        CHECK(back && hexsave::settingsFrom(*back).arcade,
              "arcade: un file con buchi riapre la partita in modalita' Arcade");
    }

    // --- A normal match stays a version 2 file ---
    {
        hexsave::SaveData data;
        data.board_size = 11;
        data.mode = GameMode::HUMAN_VS_HUMAN;
        data.red_name = "A";
        data.blue_name = "B";

        const std::string text = hexsave::serialize(data);
        CHECK(text.find("hexsave 2") != std::string::npos,
              "compatibilita': senza buchi la versione resta 2");
        CHECK(text.find("holes") == std::string::npos,
              "compatibilita': e la riga dei buchi non viene nemmeno scritta");

        std::string error;
        const std::optional<hexsave::SaveData> back = hexsave::parse(text, error);
        CHECK(back && back->holes.empty(), "compatibilita': si rilegge senza buchi");
        CHECK(back && !hexsave::settingsFrom(*back).arcade,
              "compatibilita': e non e' una partita Arcade");
    }

    // --- Older saves remain readable ---
    {
        // A file written by an earlier version, without the holes line.
        const std::string old_file =
            "hexsave 2\nsize 11\nmode human_vs_ai\ndifficulty hard\n"
            "human red\nred Elia\nblue Computer Difficile\nmoves E5 C3\n";

        std::string error;
        const std::optional<hexsave::SaveData> data = hexsave::parse(old_file, error);
        CHECK(data.has_value(), "retrocompatibilita': un file di versione 2 si legge ancora");
        CHECK(data && data->holes.empty(), "retrocompatibilita': senza buchi neri");
        CHECK(data && data->moves.size() == 2, "retrocompatibilita': le mosse ci sono tutte");
    }

    // --- Moves are validated against the holed board ---
    {
        // The file walls C3 and then claims to play on it. The replay must reject
        // that, and must do so through the same validation as any other illegal move,
        // with no special-purpose check.
        const std::string tampered =
            "hexsave 3\nsize 11\nmode human_vs_ai\ndifficulty medium\n"
            "human red\nred Elia\nblue Computer\nholes C3\nmoves C3\n";

        std::string error;
        const std::optional<hexsave::SaveData> data = hexsave::parse(tampered, error);
        CHECK(data.has_value(), "manomissione: il file si analizza");

        const std::optional<std::vector<hex::Move>> moves = hexsave::materialise(*data, error);
        CHECK(!moves.has_value(), "manomissione: una mossa dentro un buco nero viene rifiutata");
        CHECK(!error.empty(), "manomissione: il rifiuto ha un motivo");
    }

    // --- A file with repeated or off-board holes is rejected ---
    {
        std::string error;

        const std::string duplicated =
            "hexsave 3\nsize 11\nmode human_vs_ai\nholes C3 C3\nmoves\n";
        CHECK(!hexsave::parse(duplicated, error).has_value(),
              "buchi: una cella ripetuta rende il file non valido");

        const std::string outside =
            "hexsave 3\nsize 11\nmode human_vs_ai\nholes Z9\nmoves\n";
        CHECK(!hexsave::parse(outside, error).has_value(),
              "buchi: una cella fuori dalla scacchiera rende il file non valido");

        const std::string not_a_cell =
            "hexsave 3\nsize 11\nmode human_vs_ai\nholes PIE\nmoves\n";
        CHECK(!hexsave::parse(not_a_cell, error).has_value(),
              "buchi: uno scambio non e' una cella murata");
    }

    // --- A reopened Arcade match finds its own board again ---
    {
        hexsave::SaveData data;
        data.board_size = 11;
        data.mode = GameMode::HUMAN_VS_AI;
        data.red_name = "Elia";
        data.blue_name = "Computer";
        data.holes = {{0, 1}, {0, 9}, {8, 8}, {10, 3}};
        data.moves = {{hexsave::TokenKind::ADD, 5, 5}, {hexsave::TokenKind::ADD, 4, 4}};

        std::string error;
        const std::optional<hexsave::SaveData> back =
            hexsave::parse(hexsave::serialize(data), error);
        CHECK(back.has_value(), "ripristino: il file si rilegge");

        const std::optional<std::vector<hex::Move>> moves = hexsave::materialise(*back, error);
        CHECK(moves.has_value(), "ripristino: le mosse si rigiocano sulla scacchiera bucata");
        CHECK(moves && moves->size() == 2, "ripristino: sono tutte e due");
    }
}
