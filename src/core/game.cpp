/**
 * @file game.cpp
 * @brief Situation implementation: rule checks and state transitions.
 */

#include "core/game.h"
#include <stdexcept>
#include <utility>

namespace hex {
    Situation::Situation(const HexBoard& b, const Player toMove)
        : board(b), to_move(toMove), status(GameStatus::IN_PROGRESS),
          end_reason(EndReason::NONE), pie_rule(false) {
        // Count in place: materialising two position vectors just to read their
        // size would cost two allocations per state created.
        if (b.countPiece(Piece::RED_DISC) == 1 && b.countPiece(Piece::BLUE_DISC) == 0)
            pie_rule = true;
    }

    Situation::Situation(HexBoard b, const Player toMove, const GameStatus st, const EndReason reason)
        : board(std::move(b)), to_move(toMove), status(st), end_reason(reason), pie_rule(false) {
        if (st == GameStatus::IN_PROGRESS
            && board.countPiece(Piece::RED_DISC) == 1
            && board.countPiece(Piece::BLUE_DISC) == 0) {
            pie_rule = true;
        }
    }

    bool Situation::isValid(Move m) const {
        if (isOver()) return false;
        if (m.kind == MoveKind::RESIGN) return true;

        if (m.action.piece != pieceOf(to_move)) return false;
        if (!board.isValidPos(m.action.position)) return false;

        if (m.kind == MoveKind::PIE) {
            return pie_rule && board.getPieceAtPos(m.action.position) == Piece::RED_DISC;
        }

        return board.getPieceAtPos(m.action.position) == Piece::EMPTY;
    }

    std::vector<Move> Situation::validMoves() const {
        const int empty = board.countPiece(Piece::EMPTY);

        std::vector<Move> moves;
        moves.reserve(static_cast<std::size_t>(empty) + (pie_rule ? 1 : 0));

        if (pie_rule) {
            if (const auto red_pieces = board.getPosByPiece(Piece::RED_DISC); !red_pieces.empty()) {
                moves.emplace_back(MoveKind::PIE, Action(ActionKind::SWAP, Piece::BLUE_DISC, red_pieces[0]));
            }
        }

        // Single allocation for the result: empty cells are walked in place
        // rather than through an intermediate vector.
        const Piece p = pieceOf(to_move);
        board.forEachEmpty([&](const int r, const int c) {
            moves.emplace_back(MoveKind::ADD, Action(ActionKind::ADD, p, std::pair{r, c}));
        });

        return moves;
    }

    Situation Situation::next(Move m) const {
        if (!isValid(m)) throw std::invalid_argument("Invalid Move");

        HexBoard nextBoard = board;

        // Resignation awards the win to the opponent of the side to move.
        if (m.kind == MoveKind::RESIGN) {
            return {std::move(nextBoard), opponent(to_move),
                    statusFor(opponent(to_move)), EndReason::RESIGN};
        }

        // The swap counts as Blue's move, so the turn passes back to Red.
        if (m.kind == MoveKind::PIE) {
            nextBoard.swapPiece(m.action.position);
            return {std::move(nextBoard), opponent(to_move),
                    GameStatus::IN_PROGRESS, EndReason::NONE};
        }

        nextBoard.addPiece(m.action.piece, m.action.position);
        const GameStatus st = nextBoard.checkWin(to_move) ? statusFor(to_move) : GameStatus::IN_PROGRESS;
        const EndReason reason = st == GameStatus::IN_PROGRESS ? EndReason::NONE : EndReason::CONNECTION;

        return {std::move(nextBoard), opponent(to_move), st, reason};
    }

    Situation Situation::endedBy(const Player winner, const EndReason reason) const {
        return {board, opponent(to_move), statusFor(winner), reason};
    }
}
