/**
 * @file test_save_format.cpp
 * @brief Save format: parsing, and replaying matches.
 */

#include "core/game_controller.h"
#include "core/game_settings.h"
#include "core/save_format.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "test_doubles.h"
#include "core/localization.h"
#include "test_framework.h"

using namespace hex;
using namespace hexsave;
using namespace hextest;

namespace {

    /** @brief Builds a placement move for the side to move. */
    Move addFor(const Situation& s, const std::pair<int, int> pos) {
        return {MoveKind::ADD, Action(ActionKind::ADD, pieceOf(s.toMove()), pos)};
    }
}

/** @brief Default-language strings: the checks compare against that text. */
namespace { const hexui::LocalizationManager loc; }

void save_format() {
    // --- Move notation ---
    {
        CHECK(tokenToString({TokenKind::ADD, 0, 0}) == "A1", "notazione: (0,0) e' A1");
        CHECK(tokenToString({TokenKind::ADD, 10, 10}) == "K11", "notazione: (10,10) e' K11");
        CHECK(tokenToString({TokenKind::PIE, 0, 0}) == "PIE", "notazione: scambio");
        CHECK(tokenToString({TokenKind::RESIGN, 0, 0}) == "RESIGN", "notazione: resa");

        CHECK((tokenFromString("A1", 11) == MoveToken{TokenKind::ADD, 0, 0}), "notazione: A1 letto");
        CHECK((tokenFromString("K11", 11) == MoveToken{TokenKind::ADD, 10, 10}), "notazione: K11 letto");
        CHECK((tokenFromString("PIE", 11) == MoveToken{TokenKind::PIE, 0, 0}), "notazione: PIE letto");

        // Round trip across every cell of a board.
        for (int r = 0; r < 11; ++r)
            for (int c = 0; c < 11; ++c) {
                const MoveToken t{TokenKind::ADD, r, c};
                CHECK_QUIET(tokenFromString(tokenToString(t), 11) == t);
            }
        CHECK(quiet_failures == 0, "notazione: andata e ritorno su tutte le 121 celle");
        quiet_failures = 0;
    }
    {
        // Malformed or off-board tokens are refused, never guessed at.
        CHECK(tokenFromString("", 11) == std::nullopt, "notazione: token vuoto rifiutato");
        CHECK(tokenFromString("A", 11) == std::nullopt, "notazione: manca la riga");
        CHECK(tokenFromString("1", 11) == std::nullopt, "notazione: manca la colonna");
        CHECK(tokenFromString("a1", 11) == std::nullopt, "notazione: minuscole rifiutate");
        CHECK(tokenFromString("A0", 11) == std::nullopt, "notazione: riga zero rifiutata");
        CHECK(tokenFromString("A12", 11) == std::nullopt, "notazione: riga oltre la scacchiera");
        CHECK(tokenFromString("L1", 11) == std::nullopt, "notazione: colonna oltre la scacchiera");
        CHECK(tokenFromString("A-1", 11) == std::nullopt, "notazione: numero negativo rifiutato");
        CHECK(tokenFromString("PIPPO", 11) == std::nullopt, "notazione: parola sconosciuta rifiutata");
    }

    // --- File round trip ---
    {
        SaveData data;
        data.board_size = 11;
        data.mode = hexapp::GameMode::HUMAN_VS_AI;
        data.difficulty = hexapp::Difficulty::HARD;
        data.red_name = "Elia";
        data.blue_name = "IA Difficile";
        data.moves = {{TokenKind::ADD, 4, 4}, {TokenKind::PIE, 0, 0}, {TokenKind::ADD, 2, 6}};

        const std::string text = serialize(data);
        // The version declares which features the file needs, not which build wrote
        // it: without walled cells the version 2 features suffice and the file stays
        // readable by already released builds. The black hole case is covered by the
        // "profile" group.
        CHECK(text.starts_with("hexsave 2\n"),
              "file: senza buchi neri l'intestazione dichiara la versione 2");
        CHECK(text.find("\nmoves E5 PIE G3\n") != std::string::npos, "file: le mosse in notazione leggibile");

        std::string error;
        const std::optional<SaveData> back = parse(text, error);
        CHECK(back.has_value(), "file: si rilegge quello che si e' scritto");
        CHECK(error.empty(), "file: nessun errore su un file valido");
        CHECK(back->board_size == 11, "file: dimensione conservata");
        CHECK(back->mode == hexapp::GameMode::HUMAN_VS_AI, "file: modalita' conservata");
        CHECK(back->difficulty == hexapp::Difficulty::HARD, "file: difficolta' conservata");
        CHECK(back->red_name == "Elia", "file: nome del Rosso conservato");
        CHECK(back->blue_name == "IA Difficile", "file: nome del Blu conservato, spazi compresi");
        CHECK(back->moves == data.moves, "file: mosse conservate");
    }
    {
        // Names holding characters that would break the format are sanitised on write.
        SaveData data;
        data.mode = hexapp::GameMode::HUMAN_VS_HUMAN;
        data.red_name = "Eli\na\tNome";
        data.blue_name = "";

        std::string error;
        const std::optional<SaveData> back = parse(serialize(data), error);
        CHECK(back.has_value(), "file: un nome con a capo non produce un file illeggibile");
        CHECK(back->red_name.find('\n') == std::string::npos, "file: l'a capo e' stato tolto");
        CHECK(back->blue_name == "?", "file: un nome vuoto ha un ripiego");
    }


    // --- Two human players: both names survive the save ---
    {
        SaveData data;
        data.board_size = 11;
        data.mode = hexapp::GameMode::HUMAN_VS_HUMAN;
        data.red_name = "Elia";
        data.blue_name = "Marco";
        data.moves = {{TokenKind::ADD, 4, 4}, {TokenKind::ADD, 2, 6}};

        std::string error;
        const std::optional<SaveData> back = parse(serialize(data), error);
        CHECK(back.has_value(), "due umani: il file si rilegge");
        CHECK(back->red_name == "Elia", "due umani: il nome del Rosso");
        CHECK(back->blue_name == "Marco", "due umani: il nome del Blu");
        CHECK(back->mode == hexapp::GameMode::HUMAN_VS_HUMAN, "due umani: modalita' conservata");

        // The two names land in the right settings on load.
        hexapp::GameSettings settings;
        settings.mode = back->mode;
        settings.player_name = back->red_name;
        settings.opponent_name = back->blue_name;

        CHECK(hexapp::firstPlayerName(settings, loc) == "Elia", "due umani: primo giocatore ripristinato");
        CHECK(hexapp::secondPlayerName(settings, loc) == "Marco", "due umani: secondo giocatore ripristinato");
    }
    {
        // Names with accents and spaces pass through whole, byte for byte.
        SaveData data;
        data.mode = hexapp::GameMode::HUMAN_VS_HUMAN;
        data.red_name = "Nicolo' Rossi";
        data.blue_name = "Ana Maria";

        std::string error;
        const std::optional<SaveData> back = parse(serialize(data), error);
        CHECK(back->red_name == "Nicolo' Rossi", "due umani: apostrofi e spazi conservati");
        CHECK(back->blue_name == "Ana Maria", "due umani: anche nel secondo nome");
    }
    {
        // A very long name is truncated rather than written out in full.
        SaveData data;
        data.mode = hexapp::GameMode::HUMAN_VS_HUMAN;
        data.red_name = std::string(500, 'x');
        data.blue_name = "Marco";

        std::string error;
        const std::optional<SaveData> back = parse(serialize(data), error);
        CHECK(back.has_value(), "due umani: un nome smisurato non rompe il file");
        CHECK(back->red_name.size() <= MAX_NAME_LENGTH, "due umani: il nome viene troncato");
        CHECK(back->blue_name == "Marco", "due umani: il secondo nome resta leggibile");
    }

    // --- Malformed files: refused with a reason, never a crash ---
    {
        std::string error;
        CHECK(parse("", error) == std::nullopt && !error.empty(), "file rotto: vuoto");
        CHECK(parse("ciao\n", error) == std::nullopt, "file rotto: intestazione sbagliata");
        CHECK(parse("hexsave 99\nsize 11\n", error) == std::nullopt, "file rotto: versione futura");
        CHECK(parse("hexsave 1\nmode human_vs_ai\nmoves\n", error) == std::nullopt,
              "file rotto: manca la dimensione");
        CHECK(parse("hexsave 1\nsize 11\nmoves\n", error) == std::nullopt,
              "file rotto: manca la modalita'");
        CHECK(parse("hexsave 1\nsize 11\nmode human_vs_ai\n", error) == std::nullopt,
              "file rotto: manca la riga delle mosse");
        CHECK(parse("hexsave 1\nsize 0\nmode human_vs_ai\nmoves\n", error) == std::nullopt,
              "file rotto: dimensione zero");
        CHECK(parse("hexsave 1\nsize 999\nmode human_vs_ai\nmoves\n", error) == std::nullopt,
              "file rotto: dimensione oltre il massimo");
        CHECK(parse("hexsave 1\nsize 11\nmode qualcosa\nmoves\n", error) == std::nullopt,
              "file rotto: modalita' sconosciuta");
        CHECK(parse("hexsave 1\nsize 11\nmode human_vs_ai\ndifficulty estrema\nmoves\n", error) == std::nullopt,
              "file rotto: difficolta' sconosciuta");
        CHECK(parse("hexsave 1\nsize 11\nmode human_vs_ai\nmoves A1 ZZ9\n", error) == std::nullopt,
              "file rotto: mossa non riconosciuta");
    }
    {
        // An oversized move line must not be able to drive an unbounded allocation.
        std::string moves;
        for (int i = 0; i < 400; ++i) moves += " A1";

        std::string error;
        CHECK(parse("hexsave 1\nsize 11\nmode human_vs_ai\nmoves" + moves + "\n", error) == std::nullopt,
              "file rotto: piu' mosse di quante ne stiano sulla scacchiera");
    }
    {
        // Unknown keys are ignored: a file from a newer version stays readable for
        // whatever part can be understood.
        std::string error;
        const std::optional<SaveData> data =
            parse("hexsave 1\nsize 11\nmode human_vs_ai\nmoves A1\ntheme scuro\nelo 1800\n", error);
        CHECK(data.has_value(), "file: le chiavi sconosciute non fanno fallire l'analisi");
        CHECK(data->moves.size() == 1, "file: il resto viene letto normalmente");
    }

    // --- Materialisation: from tokens to real moves ---
    {
        SaveData data;
        data.board_size = 11;
        data.moves = {{TokenKind::ADD, 2, 5}, {TokenKind::PIE, 0, 0}, {TokenKind::ADD, 4, 4}};

        std::string error;
        const std::optional<std::vector<Move>> moves = materialise(data, error);
        CHECK(moves.has_value(), "materializza: sequenza valida accettata");
        CHECK(moves->size() == 3, "materializza: tre mosse");

        // The turn decides the colour, not the file.
        CHECK((*moves)[0].action.piece == Piece::RED_DISC, "materializza: la prima mossa e' del Rosso");
        CHECK((*moves)[1].kind == MoveKind::PIE, "materializza: lo scambio e' riconosciuto");
        CHECK((*moves)[1].action.position == std::make_pair(2, 5),
              "materializza: lo scambio ritrova da solo la pedina da scambiare");
        CHECK((*moves)[2].action.piece == Piece::RED_DISC,
              "materializza: dopo lo scambio tocca di nuovo al Rosso");
    }
    {
        // A cell occupied twice is the typical signature of a tampered file.
        SaveData data;
        data.board_size = 11;
        data.moves = {{TokenKind::ADD, 3, 3}, {TokenKind::ADD, 3, 3}};

        std::string error;
        CHECK(materialise(data, error) == std::nullopt, "materializza: cella occupata due volte rifiutata");
        CHECK(error.find("2") != std::string::npos, "materializza: l'errore dice quale mossa");
    }
    {
        // A swap outside its turn: there is no red stone to swap.
        SaveData data;
        data.board_size = 11;
        data.moves = {{TokenKind::PIE, 0, 0}};

        std::string error;
        CHECK(materialise(data, error) == std::nullopt, "materializza: scambio alla prima mossa rifiutato");
    }
    {
        // Moves after the match has ended.
        SaveData data;
        data.board_size = 11;
        for (int r = 0; r < 11; ++r) {
            data.moves.push_back({TokenKind::ADD, r, 0});     // colonna rossa
            if (r < 10) data.moves.push_back({TokenKind::ADD, r, 5});
        }
        data.moves.push_back({TokenKind::ADD, 0, 9});          // una di troppo

        std::string error;
        CHECK(materialise(data, error) == std::nullopt, "materializza: mosse dopo la vittoria rifiutate");
    }

    // --- Replay into the controller ---
    {
        DeferredPlayer red, blue;
        GameController c(11, red, blue);

        SaveData data;
        data.board_size = 11;
        data.moves = {{TokenKind::ADD, 2, 5}, {TokenKind::ADD, 4, 4}, {TokenKind::ADD, 6, 6}};

        std::string error;
        const std::optional<std::vector<Move>> moves = materialise(data, error);
        CHECK(moves.has_value(), "replay: sequenza materializzata");

        CHECK(c.replay(*moves), "replay: la sequenza viene applicata");
        CHECK(c.moveCount() == 3, "replay: tre mosse nella storia");
        CHECK(c.positions().size() == 4, "replay: quattro posizioni, iniziale compresa");
        CHECK(c.situation().toMove() == Player::BLUE, "replay: il turno e' quello giusto");
        CHECK(c.situation().getBoard().getPieceAtPos({2, 5}) == Piece::RED_DISC, "replay: prima pedina");
        CHECK(c.situation().getBoard().getPieceAtPos({4, 4}) == Piece::BLUE_DISC, "replay: seconda pedina");

        // After the replay the match is live: it can be undone and continued.
        CHECK(c.canUndo(), "replay: la partita ricostruita si puo' annullare");
        CHECK(c.undo(), "replay: undo dopo il caricamento");
        CHECK(c.moveCount() == 2, "replay: l'undo agisce sulla storia ricostruita");
    }
    {
        // Illegal sequence: the controller is not touched even halfway.
        DeferredPlayer red, blue;
        GameController c(11, red, blue);

        const Situation start(HexBoard(11), Player::RED);
        const Move ok1 = addFor(start, {0, 0});
        const Situation after = start.next(ok1);
        const Move ok2 = addFor(after, {1, 1});
        const Move illegal{MoveKind::ADD, Action(ActionKind::ADD, Piece::RED_DISC, {0, 0})};

        CHECK(!c.replay({ok1, ok2, illegal}), "replay: una sequenza con una mossa illegale viene rifiutata");
        CHECK(c.moveCount() == 0, "replay: nessuna mossa applicata a meta'");
        CHECK(c.situation().getBoard().getPosByPiece(Piece::RED_DISC).empty(),
              "replay: la scacchiera e' rimasta vuota");
        CHECK(!c.canUndo(), "replay: niente da annullare dopo un rifiuto");

        // And after the refusal the controller is still usable.
        CHECK(c.replay({ok1, ok2}), "replay: dopo un rifiuto si puo' riprovare");
        CHECK(c.moveCount() == 2, "replay: la seconda sequenza viene applicata");
    }
    {
        // Replay onto a match already begun: refused.
        DeferredPlayer red, blue;
        GameController c(11, red, blue);

        const Situation start(HexBoard(11), Player::RED);
        const Move m = addFor(start, {5, 5});

        CHECK(c.replay({m}), "replay: prima riproduzione accettata");
        CHECK(!c.replay({m}), "replay: su una partita gia' avviata viene rifiutata");
        CHECK(c.moveCount() == 1, "replay: la storia non e' stata alterata");
    }
    {
        // Empty sequence: legitimate, matching a game saved immediately.
        DeferredPlayer red, blue;
        GameController c(11, red, blue);
        CHECK(c.replay({}), "replay: una sequenza vuota e' accettata");
        CHECK(c.moveCount() == 0, "replay: nessuna mossa, nessun cambiamento");
    }

    // --- The full circuit: play, save, reload ---
    {
        DeferredPlayer red, blue;
        GameController original(11, red, blue);

        const std::vector<std::pair<int, int>> played{{2, 5}, {4, 4}, {6, 6}, {1, 1}, {8, 3}};
        for (const auto& pos : played) {
            red.submit(addFor(original.situation(), pos));
            blue.submit(addFor(original.situation(), pos));
            stepUntilSettled(original);
        }
        CHECK(original.moveCount() == played.size(), "giro completo: partita giocata");

        SaveData data;
        data.board_size = 11;
        data.mode = hexapp::GameMode::HUMAN_VS_HUMAN;
        data.red_name = "Elia";
        data.blue_name = "Umano 2";
        data.moves = tokensFromMoves(original.moveHistory());

        std::string error;
        const std::optional<SaveData> reloaded = parse(serialize(data), error);
        CHECK(reloaded.has_value(), "giro completo: il file si rilegge");

        const std::optional<std::vector<Move>> moves = materialise(*reloaded, error);
        CHECK(moves.has_value(), "giro completo: le mosse si materializzano");

        DeferredPlayer red2, blue2;
        GameController restored(11, red2, blue2);
        CHECK(restored.replay(*moves), "giro completo: la partita si ricostruisce");

        CHECK(restored.moveCount() == original.moveCount(), "giro completo: stesso numero di mosse");
        CHECK(restored.situation().toMove() == original.situation().toMove(),
              "giro completo: stesso giocatore di turno");
        CHECK(restored.situation().getBoardView() == original.situation().getBoardView(),
              "giro completo: scacchiera identica all'originale");
    }

    // --- Save slot names ------------------------------------------------------
    //
    // The user writes the name and it becomes a path: the only part of a save that
    // can attempt to escape the directory.
    {
        CHECK(isValidSlotName("partita 1"), "nome: lettere, cifre e spazi vanno bene");
        CHECK(isValidSlotName("finale-2_bis"), "nome: trattino e sottolineatura vanno bene");

        CHECK(!isValidSlotName(""), "nome: vuoto rifiutato");
        CHECK(!isValidSlotName(" x"), "nome: spazio iniziale rifiutato");
        CHECK(!isValidSlotName("x "), "nome: spazio finale rifiutato");
        CHECK(!isValidSlotName(".."), "nome: risalita di cartella rifiutata");
        CHECK(!isValidSlotName("../fuori"), "nome: percorso relativo rifiutato");
        CHECK(!isValidSlotName("sotto/cartella"), "nome: separatore rifiutato");
        CHECK(!isValidSlotName("C:\\assoluto"), "nome: percorso assoluto rifiutato");
        CHECK(!isValidSlotName("con.punto"), "nome: punto rifiutato");
        CHECK(!isValidSlotName(std::string(MAX_SLOT_LENGTH + 1, 'a')), "nome: troppo lungo rifiutato");

        // Normalisation strips what is not accepted rather than refusing outright:
        // somebody typing "Game!" has still said what they want it called.
        const std::optional<std::string> cleaned = normaliseSlotName("  Partita! #1  ");
        CHECK(cleaned == std::optional<std::string>{"Partita 1"},
              "nome: la ripulitura toglie i caratteri rifiutati e gli spazi ai bordi");
        CHECK(normaliseSlotName("///") == std::nullopt,
              "nome: se non resta niente di valido, non c'e' nome");

        // A refused name must not even reach the filesystem.
        std::string error;
        SaveData data;
        data.moves = {};
        CHECK(!writeToFile("../fuori", data, error), "nome: la scrittura rifiuta un nome non valido");
        CHECK(!error.empty(), "nome: la scrittura spiega il rifiuto");
        CHECK(readFromFile("../fuori", error) == std::nullopt,
              "nome: la lettura rifiuta un nome non valido");
    }

    // --- Who was playing: the human's colour ----------------------------------
    //
    // The file records the moves, but without knowing who was making them a
    // reloaded match cannot tell whose the two names are, and a rematch would swap
    // roles it does not know.
    {
        SaveData data;
        data.board_size = 11;
        data.mode = hexapp::GameMode::HUMAN_VS_AI;
        data.difficulty = hexapp::Difficulty::HARD;
        data.human_colour = Player::BLUE;
        data.red_name = "IA Difficile";
        data.blue_name = "Elia";

        const std::string text = serialize(data);
        CHECK(text.find("\nhuman blue\n") != std::string::npos, "umano: il colore finisce nel file");

        std::string error;
        const std::optional<SaveData> back = parse(text, error);
        CHECK(back.has_value(), "umano: il file si rilegge");
        CHECK(back && back->human_colour == Player::BLUE, "umano: il colore torna indietro");

        // The reconstructed settings store names by role, not by colour.
        const hexapp::GameSettings s = settingsFrom(*back);
        CHECK(s.human_colour == Player::BLUE, "ripristino: l'umano giocava Blu");
        CHECK(s.player_name == "Elia", "ripristino: il nome dell'umano e' quello del Blu");
        CHECK(s.opponent_name == "IA Difficile", "ripristino: l'avversario e' l'IA");
        CHECK(s.mode == hexapp::GameMode::HUMAN_VS_AI, "ripristino: modalita' conservata");
        CHECK(s.difficulty == hexapp::Difficulty::HARD, "ripristino: livello conservato");
        CHECK(s.board_size == 11, "ripristino: scacchiera conservata");

        // The names land back on the right colours. Without human_colour this is the
        // circuit that misassigns them, giving the human the engine's name.
        CHECK(hexapp::nameOfColour(s, Player::BLUE, loc) == "Elia",
              "ripristino: il Blu e' ancora l'umano");
        CHECK(hexapp::nameOfColour(s, Player::RED, loc) == "IA Difficile",
              "ripristino: il Rosso e' ancora l'IA");

        // And a rematch of a loaded game swaps the real roles.
        const hexapp::GameSettings next = hexapp::rematchOf(s);
        CHECK(next.human_colour == Player::RED, "ripristino: la rivincita passa l'umano al Rosso");
        CHECK(hexapp::nameOfColour(next, Player::RED, loc) == "Elia",
              "ripristino: nella rivincita apre l'umano");
        CHECK(hexapp::nameOfColour(next, Player::BLUE, loc) == "IA Difficile",
              "ripristino: e risponde l'IA");
    }
    {
        // Between two humans the first name stays Red, as in the menu.
        SaveData data;
        data.mode = hexapp::GameMode::HUMAN_VS_HUMAN;
        data.red_name = "Anna";
        data.blue_name = "Bruno";

        const hexapp::GameSettings s = settingsFrom(data);
        CHECK(s.player_name == "Anna", "ripristino fra umani: il primo nome e' il Rosso");
        CHECK(s.opponent_name == "Bruno", "ripristino fra umani: il secondo e' il Blu");
    }
    {
        // Backward compatibility: a version 1 file does not say who the human was, so
        // Red is assumed, matching what the game assigned when those files were
        // written.
        std::string error;
        const std::optional<SaveData> old_file =
            parse("hexsave 1\nsize 11\nmode human_vs_ai\nred Elia\nblue IA Medio\nmoves A1\n", error);

        CHECK(old_file.has_value(), "vecchio file: si legge ancora");
        CHECK(old_file && old_file->human_colour == Player::RED,
              "vecchio file: senza la riga si assume il Rosso");

        const hexapp::GameSettings s = settingsFrom(*old_file);
        CHECK(s.player_name == "Elia", "vecchio file: l'umano e' il Rosso, col suo nome");
        CHECK(s.opponent_name == "IA Medio", "vecchio file: l'avversario e' il Blu");

        // A misspelled colour means a broken file rather than a silent fallback: the
        // fallback covers absence, which signals an old file, not a value somebody
        // wrote that cannot be understood.
        CHECK(parse("hexsave 2\nsize 11\nmode human_vs_ai\nhuman verde\nmoves\n", error) == std::nullopt,
              "file rotto: colore dell'umano sconosciuto");
    }

    // --- Several saves in one directory ---------------------------------------
    //
    // This circuit goes through the disk: the only part of saving the other checks
    // do not touch, and the part the menu listing queries.
    {
        SaveData data;
        data.board_size = 11;
        data.mode = hexapp::GameMode::HUMAN_VS_HUMAN;
        data.red_name = "Elia";
        data.blue_name = "Umano 2";

        std::string error;
        CHECK(writeToFile("prova qa uno", data, error), "cartella: il primo file viene scritto");
        CHECK(writeToFile("prova qa due", data, error), "cartella: il secondo file viene scritto");

        const std::vector<std::string> names = listSaves();
        CHECK(std::find(names.begin(), names.end(), "prova qa uno") != names.end(),
              "cartella: il primo salvataggio compare nell'elenco");
        CHECK(std::find(names.begin(), names.end(), "prova qa due") != names.end(),
              "cartella: il secondo salvataggio compare nell'elenco");
        CHECK(std::is_sorted(names.begin(), names.end()), "cartella: l'elenco e' ordinato");

        CHECK(saveExists("prova qa uno"), "cartella: il file esiste col nome scelto");
        const std::optional<SaveData> back = readFromFile("prova qa due", error);
        CHECK(back.has_value(), "cartella: si rilegge il salvataggio indicato");
        CHECK(back && back->red_name == "Elia", "cartella: e' proprio quello scritto");

        std::error_code ec;
        std::filesystem::remove(savePath("prova qa uno"), ec);
        std::filesystem::remove(savePath("prova qa due"), ec);
    }

    // --- Deletion -------------------------------------------------------------
    {
        SaveData data;
        data.board_size = 5;
        data.mode = hexapp::GameMode::HUMAN_VS_HUMAN;
        data.red_name = "Elia";
        data.blue_name = "Umano 2";

        std::string error;
        CHECK(writeToFile("prova qa del", data, error), "cancellazione: il file di prova esiste");

        CHECK(deleteSave("prova qa del", error), "cancellazione: riuscita");
        CHECK(error.empty(), "cancellazione: nessun errore riportato");
        CHECK(!saveExists("prova qa del"), "cancellazione: il file non c'e' piu'");

        // Deleting what is not there is the intended result rather than a failure:
        // the listing on screen may be older than the disk.
        CHECK(deleteSave("prova qa del", error), "cancellazione: un file gia' assente non e' un errore");

        // Something that is not a valid name must not reach the filesystem at all.
        CHECK(!deleteSave("../fuori", error), "cancellazione: un nome illegale viene rifiutato");
        CHECK(!error.empty(), "cancellazione: il rifiuto ha un motivo");
    }

    // --- Renaming -------------------------------------------------------------
    {
        SaveData data;
        data.board_size = 5;
        data.mode = hexapp::GameMode::HUMAN_VS_HUMAN;
        data.red_name = "Elia";
        data.blue_name = "Umano 2";

        std::string error;
        CHECK(writeToFile("prova qa vecchio", data, error), "rinomina: il file di prova esiste");

        CHECK(renameSave("prova qa vecchio", "prova qa nuovo", error), "rinomina: riuscita");
        CHECK(!saveExists("prova qa vecchio"), "rinomina: il vecchio nome non esiste piu'");
        CHECK(saveExists("prova qa nuovo"), "rinomina: il nuovo nome esiste");

        // The contents are the same file: renaming does not rewrite the match.
        const std::optional<SaveData> back = readFromFile("prova qa nuovo", error);
        CHECK(back && back->red_name == "Elia", "rinomina: il contenuto e' intatto");

        // Renaming to the name already held is not a collision with oneself.
        CHECK(renameSave("prova qa nuovo", "prova qa nuovo", error),
              "rinomina: lo stesso nome riesce senza toccare niente");
        CHECK(saveExists("prova qa nuovo"), "rinomina: e il file e' ancora li'");

        // Overwriting another save would be data loss nobody asked for.
        CHECK(writeToFile("prova qa occupato", data, error), "rinomina: il secondo file esiste");
        CHECK(!renameSave("prova qa nuovo", "prova qa occupato", error),
              "rinomina: un nome gia' preso viene rifiutato");
        CHECK(saveExists("prova qa nuovo"), "rinomina: dopo il rifiuto l'originale e' intatto");
        CHECK(saveExists("prova qa occupato"), "rinomina: e nemmeno l'altro e' stato toccato");

        CHECK(!renameSave("prova qa nuovo", "sopra/fuori", error),
              "rinomina: un nome illegale viene rifiutato");
        CHECK(!renameSave("prova qa assente", "prova qa mai visto", error),
              "rinomina: un file che non c'e' non si rinomina");

        std::error_code ec;
        std::filesystem::remove(savePath("prova qa nuovo"), ec);
        std::filesystem::remove(savePath("prova qa occupato"), ec);
    }
}
