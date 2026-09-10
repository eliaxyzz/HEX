/**
 * @file save_format.cpp
 * @brief Save format implementation: notation, parsing and file handling.
 */

#include "core/save_format.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <utility>
#include <fstream>
#include <sstream>

namespace hexsave {

    namespace {
        /** @brief Directory holding the saved games. */
        constexpr const char* SAVE_DIR = "saves";

        /** @brief Save file extension. */
        constexpr const char* SAVE_EXT = ".hex";

        /** @brief Strips leading and trailing whitespace. */
        std::string trim(const std::string& s) {
            const auto first = s.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return {};

            const auto last = s.find_last_not_of(" \t\r\n");
            return s.substr(first, last - first + 1);
        }

        /** @brief Drops the characters that would break the line-based format. */
        std::string sanitiseName(const std::string& name) {
            std::string out;
            out.reserve(std::min(name.size(), MAX_NAME_LENGTH));

            for (const char c : name) {
                if (out.size() >= MAX_NAME_LENGTH) break;
                // Newlines would split one line into two and control characters
                // would corrupt the file, so they are dropped on write as well as
                // on read.
                if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) continue;
                out += c;
            }
            const std::string trimmed = trim(out);
            return trimmed.empty() ? "?" : trimmed;
        }

        std::string_view modeToken(const hexapp::GameMode m) {
            return m == hexapp::GameMode::HUMAN_VS_AI ? "human_vs_ai" : "human_vs_human";
        }

        std::optional<hexapp::GameMode> modeFromToken(const std::string& t) {
            if (t == "human_vs_ai") return hexapp::GameMode::HUMAN_VS_AI;
            if (t == "human_vs_human") return hexapp::GameMode::HUMAN_VS_HUMAN;
            return std::nullopt;
        }

        std::string_view difficultyToken(const hexapp::Difficulty d) {
            switch (d) {
                case hexapp::Difficulty::EASY:   return "easy";
                case hexapp::Difficulty::MEDIUM: return "medium";
                case hexapp::Difficulty::HARD:   return "hard";
            }
            return "medium";
        }

        std::optional<hexapp::Difficulty> difficultyFromToken(const std::string& t) {
            if (t == "easy") return hexapp::Difficulty::EASY;
            if (t == "medium") return hexapp::Difficulty::MEDIUM;
            if (t == "hard") return hexapp::Difficulty::HARD;
            return std::nullopt;
        }

        std::string_view colourToken(const hex::Player p) {
            return p == hex::Player::RED ? "red" : "blue";
        }

        std::optional<hex::Player> colourFromToken(const std::string& t) {
            if (t == "red") return hex::Player::RED;
            if (t == "blue") return hex::Player::BLUE;
            return std::nullopt;
        }

        /** @brief Parses an integer without throwing on malformed text. */
        std::optional<int> toInt(const std::string& text) {
            if (text.empty() || text.size() > 9) return std::nullopt;

            int value = 0;
            for (const char c : text) {
                if (c < '0' || c > '9') return std::nullopt;
                value = value * 10 + (c - '0');
            }
            return value;
        }
    }

    // --- Notation ------------------------------------------------------------

    std::string tokenToString(const MoveToken& token) {
        switch (token.kind) {
            case TokenKind::PIE:    return "PIE";
            case TokenKind::RESIGN: return "RESIGN";
            default:                break;
        }
        return std::string(1, static_cast<char>('A' + token.col)) + std::to_string(token.row + 1);
    }

    std::optional<MoveToken> tokenFromString(const std::string& text, const int board_size) {
        if (text == "PIE") return MoveToken{TokenKind::PIE, 0, 0};
        if (text == "RESIGN") return MoveToken{TokenKind::RESIGN, 0, 0};

        if (text.size() < 2 || text.size() > 3) return std::nullopt;

        const char letter = text[0];
        if (letter < 'A' || letter > 'Z') return std::nullopt;

        const std::optional<int> row_number = toInt(text.substr(1));
        if (!row_number) return std::nullopt;

        const int col = letter - 'A';
        const int row = *row_number - 1;
        if (row < 0 || row >= board_size || col >= board_size) return std::nullopt;

        return MoveToken{TokenKind::ADD, row, col};
    }

    std::vector<MoveToken> tokensFromMoves(const std::vector<hex::Move>& moves) {
        std::vector<MoveToken> tokens;
        tokens.reserve(moves.size());

        for (const hex::Move& m : moves) {
            switch (m.kind) {
                case hex::MoveKind::PIE:
                    tokens.push_back({TokenKind::PIE, 0, 0});
                    break;
                case hex::MoveKind::RESIGN:
                    tokens.push_back({TokenKind::RESIGN, 0, 0});
                    break;
                default:
                    tokens.push_back({TokenKind::ADD, m.action.position.first, m.action.position.second});
                    break;
            }
        }
        return tokens;
    }

    // --- Serialisation -------------------------------------------------------

    std::string serialize(const SaveData& data) {
        std::ostringstream out;

        // The version declares which features the file needs: a game without
        // walled cells stays at version 2, which already released builds can read.
        const int version = data.holes.empty() ? 2 : HOLES_FORMAT_VERSION;

        out << "hexsave " << version << "\n"
            << "size " << data.board_size << "\n"
            << "mode " << modeToken(data.mode) << "\n"
            << "difficulty " << difficultyToken(data.difficulty) << "\n"
            << "human " << colourToken(data.human_colour) << "\n"
            << "red " << sanitiseName(data.red_name) << "\n"
            << "blue " << sanitiseName(data.blue_name) << "\n";

        // Walled cells before the moves: that is the order they must be reapplied
        // in, and a file reads better when it reports events in order.
        if (!data.holes.empty()) {
            out << "holes";
            for (const std::pair<int, int>& h : data.holes)
                out << " " << tokenToString({TokenKind::ADD, h.first, h.second});
            out << "\n";
        }

        out << "moves";

        for (const MoveToken& t : data.moves) out << " " << tokenToString(t);
        out << "\n";

        return out.str();
    }

    std::optional<SaveData> parse(const std::string& text, std::string& error) {
        error.clear();

        std::istringstream in(text);
        std::string line;

        // --- Header ---
        if (!std::getline(in, line)) {
            error = "file vuoto";
            return std::nullopt;
        }
        if (line.size() > MAX_LINE_LENGTH) {
            error = "riga troppo lunga";
            return std::nullopt;
        }
        {
            const std::string header = trim(line);
            if (!header.starts_with("hexsave ")) {
                error = "non e' un file di salvataggio Hex";
                return std::nullopt;
            }
            // A range of versions is accepted rather than a single one, so files
            // already written stay readable as the format grows. That is the only
            // reason to stamp a version at all.
            const std::optional<int> version = toInt(trim(header.substr(8)));
            if (!version || *version < MIN_FORMAT_VERSION || *version > FORMAT_VERSION) {
                error = "versione del formato non supportata";
                return std::nullopt;
            }
        }

        SaveData data;
        bool has_size = false, has_mode = false, has_moves = false;
        std::string moves_line;
        std::string holes_line;

        // --- Body: key, space, value ---
        while (std::getline(in, line)) {
            if (line.size() > MAX_LINE_LENGTH) {
                error = "riga troppo lunga";
                return std::nullopt;
            }

            const std::string trimmed = trim(line);
            if (trimmed.empty()) continue;

            const auto space = trimmed.find(' ');
            const std::string key = trimmed.substr(0, space);
            const std::string value = (space == std::string::npos) ? "" : trim(trimmed.substr(space + 1));

            if (key == "size") {
                const std::optional<int> n = toInt(value);
                if (!n || *n < MIN_BOARD_SIZE || *n > MAX_BOARD_SIZE) {
                    error = "dimensione della scacchiera non valida";
                    return std::nullopt;
                }
                data.board_size = *n;
                has_size = true;
            } else if (key == "mode") {
                const std::optional<hexapp::GameMode> m = modeFromToken(value);
                if (!m) {
                    error = "modalita' sconosciuta";
                    return std::nullopt;
                }
                data.mode = *m;
                has_mode = true;
            } else if (key == "difficulty") {
                const std::optional<hexapp::Difficulty> d = difficultyFromToken(value);
                if (!d) {
                    error = "livello di difficolta' sconosciuto";
                    return std::nullopt;
                }
                data.difficulty = *d;
            } else if (key == "human") {
                const std::optional<hex::Player> p = colourFromToken(value);
                if (!p) {
                    error = "colore dell'umano sconosciuto";
                    return std::nullopt;
                }
                data.human_colour = *p;
            } else if (key == "red") {
                data.red_name = value.substr(0, MAX_NAME_LENGTH);
            } else if (key == "blue") {
                data.blue_name = value.substr(0, MAX_NAME_LENGTH);
            } else if (key == "holes") {
                holes_line = value;
            } else if (key == "moves") {
                moves_line = value;
                has_moves = true;
            }
            // Unknown keys are ignored, so a file from a newer build stays
            // readable for whatever this one understands.
        }

        if (!has_size || !has_mode || !has_moves) {
            error = "file incompleto";
            return std::nullopt;
        }

        // --- Walled cells ---
        // Same notation as the moves and the same distrust: only real, in-bounds,
        // non-repeated cells are accepted. A duplicate would not be harmless, since
        // walling the same cell twice fails once loading is already under way
        // instead of here.
        {
            std::istringstream cells(holes_line);
            std::string cell;
            std::set<std::pair<int, int>> seen;

            const std::size_t max_holes =
                static_cast<std::size_t>(data.board_size) * data.board_size;

            while (cells >> cell) {
                if (data.holes.size() >= max_holes) {
                    error = "troppe celle murate per questa scacchiera";
                    return std::nullopt;
                }

                const std::optional<MoveToken> parsed = tokenFromString(cell, data.board_size);
                if (!parsed || parsed->kind != TokenKind::ADD) {
                    error = "cella murata non riconosciuta: " + cell;
                    return std::nullopt;
                }

                const std::pair<int, int> pos{parsed->row, parsed->col};
                if (!seen.insert(pos).second) {
                    error = "cella murata ripetuta: " + cell;
                    return std::nullopt;
                }
                data.holes.push_back(pos);
            }
        }

        // --- Moves ---
        // The ceiling is the board capacity plus the swap and the resignation;
        // beyond that the file describes an impossible game.
        const std::size_t max_moves = static_cast<std::size_t>(data.board_size) * data.board_size + 2;

        std::istringstream tokens(moves_line);
        std::string token;
        while (tokens >> token) {
            if (data.moves.size() >= max_moves) {
                error = "troppe mosse per questa scacchiera";
                return std::nullopt;
            }
            const std::optional<MoveToken> parsed = tokenFromString(token, data.board_size);
            if (!parsed) {
                error = "mossa non riconosciuta: " + token;
                return std::nullopt;
            }
            data.moves.push_back(*parsed);
        }

        return data;
    }

    std::optional<hex::Move> moveFromToken(const hex::Situation& situation,
                                           const MoveToken& token, std::string& error) {
        error.clear();

        switch (token.kind) {
            case TokenKind::RESIGN:
                return hex::Move{};   // default construction is a resignation

            case TokenKind::PIE: {
                // The swap targets the only red stone on the board; the sender does
                // not record its position and therefore cannot lie about it.
                const auto reds = situation.getBoard().getPosByPiece(hex::Piece::RED_DISC);
                if (reds.size() != 1) {
                    error = "scambio impossibile in questa posizione";
                    return std::nullopt;
                }
                return hex::Move{hex::MoveKind::PIE,
                                 hex::Action(hex::ActionKind::SWAP, hex::Piece::BLUE_DISC, reds[0])};
            }

            default:
                // The colour comes from the turn, not from the token.
                return hex::Move{hex::MoveKind::ADD,
                                 hex::Action(hex::ActionKind::ADD,
                                             pieceOf(situation.toMove()),
                                             {token.row, token.col})};
        }
    }

    std::optional<std::vector<hex::Move>> materialise(const SaveData& data, std::string& error) {
        error.clear();

        // Black holes are walled off before the replay, so from here on move
        // validation is the ordinary one and a stone the file tries to place inside
        // a hole is rejected by isValid() with no special-purpose check.
        hex::HexBoard board(data.board_size);
        for (const std::pair<int, int>& h : data.holes) {
            if (!board.isValidPos(h) || board.getPieceAtPos(h) != hex::Piece::EMPTY) {
                error = "cella murata fuori dalla scacchiera o ripetuta";
                return std::nullopt;
            }
            board.blockCell(h);
        }

        hex::Situation s(std::move(board), hex::Player::RED);
        std::vector<hex::Move> moves;
        moves.reserve(data.moves.size());

        for (std::size_t i = 0; i < data.moves.size(); ++i) {
            const MoveToken& token = data.moves[i];

            if (s.isOver()) {
                error = "il file contiene mosse dopo la fine della partita";
                return std::nullopt;
            }

            std::string why;
            const std::optional<hex::Move> translated = moveFromToken(s, token, why);
            if (!translated) {
                error = why + " alla mossa " + std::to_string(i + 1);
                return std::nullopt;
            }
            const hex::Move m = *translated;

            if (!s.isValid(m)) {
                error = "mossa illegale alla posizione " + std::to_string(i + 1);
                return std::nullopt;
            }

            s = s.next(m);
            moves.push_back(m);
        }

        return moves;
    }

    hexapp::GameSettings settingsFrom(const SaveData& data) {
        hexapp::GameSettings s;
        s.mode = data.mode;
        s.difficulty = data.difficulty;
        s.board_size = data.board_size;
        s.human_colour = data.human_colour;

        // Between two humans the first name is Red by convention; against the
        // engine it belongs to whoever configured the match, who may be Blue. Either
        // way the question is the same: which of the two names in the file is the
        // settings' first player.
        const bool human_is_red = data.mode == hexapp::GameMode::HUMAN_VS_HUMAN
                               || data.human_colour == hex::Player::RED;

        s.player_name = human_is_red ? data.red_name : data.blue_name;
        s.opponent_name = human_is_red ? data.blue_name : data.red_name;

        // Arcade is inferred from the black holes, its only trace in the file. A
        // separate `arcade` field could contradict the list, "arcade=1" with no
        // holes or holes with no flag, and something would then have to decide which
        // to believe. Inferring removes the question, and the mode switches the
        // Blitz clock back on by itself.
        s.arcade = !data.holes.empty();

        return s;
    }

    // --- Files ---------------------------------------------------------------

    bool isValidSlotName(const std::string& slot) {
        if (slot.empty() || slot.size() > MAX_SLOT_LENGTH) return false;

        // No dots and no separators: a name stays a name and "../x" is not
        // expressible. An allowlist rather than a denylist, because the ways out of
        // a directory outnumber the characters needed to name a game.
        for (const unsigned char c : slot) {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                         || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_';
            if (!ok) return false;
        }

        // Edge whitespace would produce two names indistinguishable on screen.
        return slot.front() != ' ' && slot.back() != ' ';
    }

    std::optional<std::string> normaliseSlotName(const std::string& slot) {
        std::string out;
        out.reserve(std::min(slot.size(), MAX_SLOT_LENGTH));

        for (const unsigned char c : slot) {
            if (out.size() >= MAX_SLOT_LENGTH) break;
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                         || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_';
            if (ok) out += static_cast<char>(c);
        }

        const std::string trimmed = trim(out);
        if (!isValidSlotName(trimmed)) return std::nullopt;
        return trimmed;
    }

    std::string savePath(const std::string& slot) {
        return (std::filesystem::path(SAVE_DIR) / (slot + SAVE_EXT)).string();
    }

    bool saveExists(const std::string& slot) {
        if (!isValidSlotName(slot)) return false;

        std::error_code ec;
        return std::filesystem::is_regular_file(savePath(slot), ec);
    }

    std::vector<std::string> listSaves() {
        std::vector<std::string> names;

        std::error_code ec;
        const std::filesystem::directory_iterator end;
        std::filesystem::directory_iterator it(SAVE_DIR, ec);
        if (ec) return names;   // missing directory: no saves, not an error

        for (; it != end; it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file(ec) || ec) continue;

            const std::filesystem::path& p = it->path();
            if (p.extension() != SAVE_EXT) continue;

            // A hand-placed file may carry a name the game could not rewrite. It is
            // left out, since the listing exists to feed the other functions.
            const std::string stem = p.stem().string();
            if (isValidSlotName(stem)) names.push_back(stem);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    bool writeToFile(const std::string& slot, const SaveData& data, std::string& error) {
        error.clear();

        if (!isValidSlotName(slot)) {
            error = "nome del salvataggio non valido";
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(SAVE_DIR, ec);
        if (ec) {
            error = "impossibile creare la cartella dei salvataggi";
            return false;
        }

        std::ofstream out(savePath(slot), std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "impossibile aprire il file in scrittura";
            return false;
        }

        out << serialize(data);
        if (!out) {
            error = "scrittura non riuscita";
            return false;
        }
        return true;
    }

    std::optional<SaveData> readFromFile(const std::string& slot, std::string& error) {
        error.clear();

        if (!isValidSlotName(slot)) {
            error = "nome del salvataggio non valido";
            return std::nullopt;
        }

        std::ifstream in(savePath(slot), std::ios::binary);
        if (!in) {
            error = "nessuna partita salvata";
            return std::nullopt;
        }

        std::ostringstream buffer;
        buffer << in.rdbuf();
        return parse(buffer.str(), error);
    }

    bool deleteSave(const std::string& slot, std::string& error) {
        error.clear();

        if (!isValidSlotName(slot)) {
            error = "nome del salvataggio non valido";
            return false;
        }

        std::error_code ec;
        // remove() also returns false when the file simply was not there, so the
        // error code decides, not the return value. An already absent save is the
        // intended outcome, not a failure worth reporting.
        std::filesystem::remove(savePath(slot), ec);
        if (ec) {
            error = "cancellazione non riuscita";
            return false;
        }
        return true;
    }

    bool renameSave(const std::string& from, const std::string& to, std::string& error) {
        error.clear();

        if (!isValidSlotName(from) || !isValidSlotName(to)) {
            error = "nome del salvataggio non valido";
            return false;
        }

        // Renaming a file to the name it already has succeeds and changes nothing;
        // without this case the check below would read it as a collision with
        // itself.
        if (from == to) return true;

        if (saveExists(to)) {
            error = "esiste gia' un salvataggio con questo nome";
            return false;
        }

        std::error_code ec;
        std::filesystem::rename(savePath(from), savePath(to), ec);
        if (ec) {
            error = "rinomina non riuscita";
            return false;
        }
        return true;
    }
}
