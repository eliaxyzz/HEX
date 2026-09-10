/**
 * @file sfml_game_observer.cpp
 * @brief Graphical observer implementation.
 */

#include "ui/sfml_game_observer.h"

#include "ui/console_renderer.h"

namespace hexgui {

    namespace {
        /**
         * @brief Returns a player's label in the interface language.
         * @note Deliberately not ConsoleRenderer::formatPlayer(): that one serves the
         * console executable, whose output is a development tool rather than
         * interface text. The two have different audiences and are kept apart.
         */
        const std::string& playerLabel(const hexui::LocalizationManager& loc, const hex::Player p) {
            return loc.text(p == hex::Player::RED ? hexui::StringKey::COLOUR_RED_UPPER
                                                  : hexui::StringKey::COLOUR_BLUE_UPPER);
        }

        /** @brief Returns why the match ended, in the selected language. */
        const std::string& reasonLabel(const hexui::LocalizationManager& loc, const hex::EndReason r) {
            switch (r) {
                case hex::EndReason::CONNECTION:   return loc.text(hexui::StringKey::REASON_CONNECTION);
                case hex::EndReason::RESIGN:       return loc.text(hexui::StringKey::REASON_RESIGN);
                case hex::EndReason::TIMEOUT:      return loc.text(hexui::StringKey::REASON_TIMEOUT);
                case hex::EndReason::ILLEGAL_MOVE: return loc.text(hexui::StringKey::REASON_ILLEGAL);
                default:                           return loc.text(hexui::StringKey::REASON_UNKNOWN);
            }
        }
    }

    void SfmlGameObserver::onGameStart(const hex::Situation& initial) {
        last_move.reset();
        notice.clear();
        outcome.reset();
        status = loc.text(hexui::StringKey::STATUS_TO_MOVE) + " "
               + playerLabel(loc, initial.toMove());
        dirty = true;
    }

    void SfmlGameObserver::onTurnStart(const hex::Situation&, const hex::Player mover,
                                       const std::string& playerName) {
        current_player = playerName;
        status = loc.text(hexui::StringKey::STATUS_TURN_OF) + " "
               + playerLabel(loc, mover) + " - " + playerName;
        dirty = true;
    }

    void SfmlGameObserver::onMove(const hex::Situation&, const hex::Move& m, const hex::Situation&) {
        // A swap has no sensible "last move" cell to highlight, and a resignation
        // does not touch the board at all.
        if (m.kind == hex::MoveKind::ADD) last_move = m.action.position;
        else                              last_move.reset();

        if (m.kind == hex::MoveKind::PIE) notice = loc.text(hexui::StringKey::NOTICE_PIE_SWAPPED);
        dirty = true;
    }

    void SfmlGameObserver::onUndo(const hex::Situation& restored,
                                  const std::optional<hex::Move>&) {
        last_move.reset();
        outcome.reset();

        // Deliberately no message. An undo is not an interface action but the way
        // a restart walks the history backwards, an engine detail. Reporting it would
        // put on screen the running commentary of a function the player never
        // invoked. The overlay carries the turn and the match state, not the steps
        // taken to reach them.
        notice.clear();

        status = loc.text(hexui::StringKey::STATUS_TO_MOVE) + " "
               + playerLabel(loc, restored.toMove());
        dirty = true;
    }

    void SfmlGameObserver::onInvalidMove(const hex::Situation&, hex::Player, const hex::Move& m,
                                         const std::string& playerName) {
        notice = loc.text(hexui::StringKey::NOTICE_ILLEGAL_MOVE) + " " + playerName + ": "
               + hex::ConsoleRenderer::formatMove(m);
        dirty = true;
    }

    void SfmlGameObserver::onTimeout(const hex::Situation&, hex::Player, const std::string& playerName) {
        notice = loc.text(hexui::StringKey::NOTICE_TIMEOUT) + " " + playerName;
        dirty = true;
    }

    void SfmlGameObserver::onPlayerError(const hex::Situation&, hex::Player,
                                         const std::string& playerName, const std::string& what) {
        notice = loc.text(hexui::StringKey::NOTICE_PLAYER_ERROR) + " " + playerName + ": " + what;
        dirty = true;
    }

    void SfmlGameObserver::onGameEnd(const hex::Situation&, const hex::GameResult& result) {
        outcome = result;
        status = loc.text(hexui::StringKey::STATUS_WON) + " " + playerLabel(loc, result.winner)
               + " (" + reasonLabel(loc, result.reason) + ")";
        dirty = true;
    }
}
