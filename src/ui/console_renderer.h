/**
 * @file console_renderer.h
 * @brief Text view: renders the model as text and prints it.
 */

#ifndef CONSOLE_RENDERER_H
#define CONSOLE_RENDERER_H

#include "core/board.h"
#include "core/game.h"
#include "core/game_observer.h"
#include "core/move.h"

#include <iosfwd>
#include <string>
#include <utility>

namespace hex {

    /**
     * @brief Text rendering of the board and of the game entities.
     *
     * Formatting is separated from output: the format* methods are pure functions
     * returning strings, testable without capturing std::cout, while render() only
     * writes to a stream.
     */
    class ConsoleRenderer {
    public:
        /** @brief Sentinel meaning "no cell to highlight". */
        static constexpr std::pair<int, int> NO_HIGHLIGHT{-1, -1};

        /**
         * @brief Builds a view writing to the given stream.
         * @param out Destination stream.
         */
        explicit ConsoleRenderer(std::ostream& out);

        /** @brief Builds a view writing to std::cout. */
        ConsoleRenderer();

        /**
         * @brief Prints the board to the view's stream.
         * @param board Board to draw.
         * @param last_move Optional cell to highlight in parentheses.
         */
        void render(const HexBoard& board, const std::pair<int, int>& last_move = NO_HIGHLIGHT) const;

        /**
         * @brief Composes the board's text representation.
         *
         * Includes the column header, the row numbers and the rhombus indentation.
         *
         * @param board Board to format.
         * @param last_move Optional cell to highlight in parentheses.
         * @return The complete drawing, newline terminated.
         */
        [[nodiscard]] static std::string formatBoard(const HexBoard& board,
                                                     const std::pair<int, int>& last_move = NO_HIGHLIGHT);

        /**
         * @brief Formats a move in readable notation, e.g. {0,0} becomes "A1".
         * @param m Move to format.
         */
        [[nodiscard]] static std::string formatMove(const Move& m);

        /**
         * @brief Returns a player's display name.
         */
        [[nodiscard]] static std::string formatPlayer(Player p);

        /**
         * @brief Returns the match's end reason in readable form.
         */
        [[nodiscard]] static std::string formatEndReason(EndReason r);

    private:
        /** @brief Stream the view writes to. Non-owning. */
        std::ostream* out;
    };

    /**
     * @brief Observer that narrates a match on a terminal.
     *
     * A plain GameController client: it receives the events and renders them
     * through ConsoleRenderer. All console output originates here rather than
     * inside the engine.
     */
    class ConsoleGameObserver final : public GameObserver {
    public:
        /**
         * @brief Builds the console observer.
         * @param redName Red player's name, used in the closing message.
         * @param blueName Blue player's name, used in the closing message.
         * @param out Destination stream.
         */
        ConsoleGameObserver(std::string redName, std::string blueName, std::ostream& out);

        /** @brief As above, writing to std::cout. */
        ConsoleGameObserver(std::string redName, std::string blueName);

        void onGameStart(const Situation& initial) override;
        void onTurnStart(const Situation& current, Player mover, const std::string& playerName) override;
        void onMove(const Situation& before, const Move& m, const Situation& after) override;
        void onInvalidMove(const Situation& current, Player mover, const Move& m,
                           const std::string& playerName) override;
        void onTimeout(const Situation& current, Player mover, const std::string& playerName) override;
        void onPlayerError(const Situation& current, Player mover,
                           const std::string& playerName, const std::string& what) override;
        void onUndo(const Situation& restored, const std::optional<Move>& undone) override;
        void onGameEnd(const Situation& final, const GameResult& result) override;

    private:
        /** @brief The two player names, for the victory message. */
        std::string red_name;
        std::string blue_name;

        /** @brief Destination stream. Non-owning. */
        std::ostream* out;

        /** @brief View used to draw the board. */
        ConsoleRenderer view;
    };
}

#endif //CONSOLE_RENDERER_H
