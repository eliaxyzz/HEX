/**
 * @file console_renderer.cpp
 * @brief Text view implementation.
 */

#include "ui/console_renderer.h"

#include <iostream>
#include <sstream>
#include <utility>

namespace hex {

    ConsoleRenderer::ConsoleRenderer(std::ostream& out) : out(&out) {}

    ConsoleRenderer::ConsoleRenderer() : out(&std::cout) {}

    void ConsoleRenderer::render(const HexBoard& board, const std::pair<int, int>& last_move) const {
        *out << formatBoard(board, last_move);
    }

    std::string ConsoleRenderer::formatBoard(const HexBoard& board, const std::pair<int, int>& last_move) {
        const int size = board.getSize();
        const auto cells = board.getBoardView();

        std::ostringstream os;

        // Column header: A B C ...
        os << "   ";
        for (int col = 0; col < size; ++col) {
            os << " " << static_cast<char>('A' + col) << " ";
        }
        os << "\n";

        // Board rows.
        for (int row = 0; row < size; ++row) {
            // Rhombus indentation.
            os << std::string(row * 2, ' ');

            // Row number, right-aligned to two digits.
            os << (row + 1) << (row < 9 ? "  " : " ");

            // Cells.
            for (int col = 0; col < size; ++col) {
                const Piece p = cells[row][col];
                const bool isLast = (row == last_move.first && col == last_move.second);

                if (p == Piece::RED_DISC)       os << (isLast ? "(X)" : " X ");
                else if (p == Piece::BLUE_DISC) os << (isLast ? "(O)" : " O ");
                else if (p == Piece::BLOCKED)   os << " # ";   // cella murata
                else                            os << " . ";
            }
            os << "\n";
        }

        return os.str();
    }

    std::string ConsoleRenderer::formatMove(const Move& m) {
        if (m.kind == MoveKind::RESIGN) return "RESIGN";
        if (m.kind == MoveKind::PIE) return "PIE (SWAP)";

        const char col = static_cast<char>('A' + m.action.position.second);
        // +1 because rows are numbered from one for humans and from zero in storage.
        return std::string(1, col) + std::to_string(m.action.position.first + 1);
    }

    std::string ConsoleRenderer::formatPlayer(const Player p) {
        return p == Player::RED ? "ROSSO" : "BLU";
    }

    std::string ConsoleRenderer::formatEndReason(const EndReason r) {
        switch (r) {
            case EndReason::CONNECTION:   return "connessione";
            case EndReason::RESIGN:       return "resa";
            case EndReason::TIMEOUT:      return "tempo scaduto";
            case EndReason::ILLEGAL_MOVE: return "mossa illegale";
            default:                      return "sconosciuto";
        }
    }

    // --- ConsoleGameObserver ---

    ConsoleGameObserver::ConsoleGameObserver(std::string redName, std::string blueName, std::ostream& out)
        : red_name(std::move(redName)), blue_name(std::move(blueName)), out(&out), view(out) {}

    ConsoleGameObserver::ConsoleGameObserver(std::string redName, std::string blueName)
        : ConsoleGameObserver(std::move(redName), std::move(blueName), std::cout) {}

    void ConsoleGameObserver::onGameStart(const Situation& initial) {
        view.render(initial.getBoard());
    }

    void ConsoleGameObserver::onTurnStart(const Situation&, const Player mover,
                                          const std::string& playerName) {
        *out << "\n--> Turno di " << ConsoleRenderer::formatPlayer(mover)
             << " [" << playerName << "]...\n";
    }

    void ConsoleGameObserver::onMove(const Situation&, const Move& m, const Situation& after) {
        *out << "Mossa scelta: " << ConsoleRenderer::formatMove(m) << "\n";

        if (m.kind == MoveKind::PIE) {
            *out << "!!! PIE RULE (Scambio colori) !!!\n";
            view.render(after.getBoard());
        } else if (m.kind == MoveKind::ADD) {
            view.render(after.getBoard(), m.action.position);
        }
    }

    void ConsoleGameObserver::onInvalidMove(const Situation&, Player, const Move& m,
                                            const std::string& playerName) {
        *out << "Mossa invalida di " << playerName
             << " (" << ConsoleRenderer::formatMove(m) << ")\n";
    }

    void ConsoleGameObserver::onTimeout(const Situation&, Player, const std::string& playerName) {
        *out << "Timeout per " << playerName << "\n";
    }

    void ConsoleGameObserver::onPlayerError(const Situation&, Player,
                                            const std::string& playerName, const std::string& what) {
        *out << "Errore nel calcolo della mossa di " << playerName << ": " << what << "\n";
    }

    void ConsoleGameObserver::onUndo(const Situation& restored, const std::optional<Move>& undone) {
        *out << "\n<-- Annullata: "
             << (undone ? ConsoleRenderer::formatMove(*undone) : std::string("fine partita"))
             << "\n";
        view.render(restored.getBoard());
    }

    void ConsoleGameObserver::onGameEnd(const Situation&, const GameResult& result) {
        const bool red_won = result.winner == Player::RED;
        const std::string& name = red_won ? red_name : blue_name;

        *out << "\n----------------------------------------\n"
             << "!!! VINCITORE: " << (red_won ? "ROSSO" : "BLU") << " (" << name << ") !!!"
             << " [" << ConsoleRenderer::formatEndReason(result.reason) << "]\n"
             << "----------------------------------------\n";
    }
}
