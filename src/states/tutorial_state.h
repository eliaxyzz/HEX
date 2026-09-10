/**
 * @file tutorial_state.h
 * @brief Guided onboarding: the goal is learned by playing it, not by reading it.
 *
 * Hex takes one sentence to explain, which is exactly why a page of text would be
 * wasted: a newcomer does not need to read a rule, they need to watch it happen.
 * The tutorial walks the player through the shortest sequence that produces a win
 * and explains it as it occurs.
 *
 * ### Not a match
 *
 * No GameController, no opponent and no turn: all apparatus for something that
 * needs none. There is a HexBoard the script places stones on, and an index saying
 * which step is current. A board and a win rule suffice, and they are the ones the
 * real game uses, so what is learned here holds there too.
 *
 * ### One accepted input
 *
 * Each step has exactly one valid cell, the highlighted one. A click anywhere else
 * is not an error to report, it is simply ignored: a tutorial that scolds teaches
 * fear of the interface rather than use of it.
 *
 * ### Four chapters, one mechanism
 *
 * The tutorial is a sequence of chapters of two kinds:
 *
 *  - **played** (Red, Blue): they carry a script of cells to touch and are learned
 *    by doing;
 *  - **explained** (the pie rule, Arcade): they carry no script and are read on a
 *    board arranged to illustrate what they say.
 *
 * These are not two modes of operation. An explained chapter is simply one whose
 * script is empty: finished() is then true from the first instant, the button that
 * moves on appears immediately, and everything else — drawing, text, navigation —
 * is the same code. A separate screen for the text would have duplicated the
 * navigation without sharing a line of it.
 *
 * What distinguishes one chapter from another therefore lives entirely in three
 * tables (scriptFor, boardFor and the string keys), not in the flow.
 *
 * @note The pie rule gets a chapter of its own because it is the one Hex rule that
 * looks like a defect when met without explanation: the stone just played changes
 * colour by itself, and a player who does not know the rule reads that as a bug.
 */

#ifndef TUTORIAL_STATE_H
#define TUTORIAL_STATE_H

#include <optional>
#include <utility>
#include <vector>

#include "core/board.h"
#include "core/localization.h"
#include "states/sfml_app_state.h"
#include "ui/sfml_renderer.h"
#include "ui/ui_widgets.h"

namespace hexapp {

    /** @brief Tutorial board side: the smallest one that still shows a chain. */
    inline constexpr int TUTORIAL_BOARD = 5;

    /** @brief The tutorial chapters, in the order they are walked through. */
    enum class TutorialChapter {
        RED_GOAL,   ///< Played: connect the top edge to the bottom one.
        BLUE_GOAL,  ///< Played: connect the left edge to the right one.
        PIE_RULE,   ///< Explained: why the first stone changes colour.
        ARCADE      ///< Explained: Blitz and black holes.
    };

    /**
     * @brief One script step: a cell to touch and what to explain.
     * @note Data rather than code: adding a step means adding a table row.
     */
    struct TutorialStep {
        /** @brief The only cell that accepts a click on this step. */
        std::pair<int, int> cell;

        /** @brief Phrase shown while that click is awaited. */
        hexui::StringKey text;
    };

    /** @brief Chapter-based onboarding screen. */
    class TutorialState final : public SfmlAppState {
    public:
        /** @brief Builds the tutorial on the shared context, at the first chapter. */
        explicit TutorialState(AppContext& context);

        [[nodiscard]] StateId id() const override { return StateId::TUTORIAL; }

        void handleInput(const InputEvent& event) override;
        void update(float dt) override;
        void draw(sf::RenderTarget& target) override;

        /**
         * @brief Returns the script of the given chapter, identical on every run.
         * @note Empty for explained chapters, which is what makes them explained.
         */
        [[nodiscard]] static const std::vector<TutorialStep>& scriptFor(TutorialChapter chapter);

        /** @brief Returns the current chapter's script. */
        [[nodiscard]] const std::vector<TutorialStep>& script() const { return scriptFor(chapter); }

        /**
         * @brief Opens the given chapter on a clean, arranged board.
         *
         * This is how one chapter gives way to the next: not a transition to another
         * screen, which would rebuild board, renderer and commands just to swap three
         * tables, but a reset of what the previous chapter produced. The constructor
         * calls it for the first chapter too, rather than repeating the setup.
         */
        void startChapter(TutorialChapter chapter);

        /** @brief Returns the current chapter. */
        [[nodiscard]] TutorialChapter currentChapter() const { return chapter; }

        /** @brief Returns the following chapter, unless this is the last. */
        [[nodiscard]] static std::optional<TutorialChapter> nextAfter(TutorialChapter chapter);

        /** @brief Returns the current step; equals the script length once finished. */
        [[nodiscard]] std::size_t stepIndex() const { return step; }

        /**
         * @brief Tests whether no further click is awaited.
         * @note True from the outset for an explained chapter: there is nothing to
         * touch.
         */
        [[nodiscard]] bool finished() const { return step >= script().size(); }

    private:
        /** @brief Shared application context. Non-owning. */
        AppContext& context;

        /** @brief Current chapter; selects script, board, colour and strings. */
        TutorialChapter chapter = TutorialChapter::RED_GOAL;

        /** @brief The chapter's board, filled by the tutorial rather than an engine. */
        hex::HexBoard board;

        /** @brief Board renderer, the same one the real match uses. */
        hexgui::SfmlBoardRenderer renderer;

        /** @brief Current step in the script. */
        std::size_t step = 0;

        /** @brief Seconds since entry; drives the pulse of the cell to touch. */
        float elapsed = 0.0f;

        /** @brief Cell under the pointer, when the pointer is over the board. */
        std::optional<std::pair<int, int>> hover;

        /** @brief Returns to the menu. */
        hexui::Button back;

        /**
         * @brief Moves to the next chapter.
         * @note Exists only where pressing it makes sense: once the chapter is done,
         * and never on the last one.
         */
        hexui::Button next;

        /**
         * @brief Winning chain, computed when a script completes.
         * @note Reuses the real match's search, so the win the tutorial shows is
         * recognised by the same code that recognises one in a game, not by a special
         * case written here.
         */
        std::vector<std::pair<int, int>> winning_path;

        /** @brief Seconds since the chain appeared. */
        float win_elapsed = 0.0f;

        /** @brief Returns the cell the current step awaits, if the script is unfinished. */
        [[nodiscard]] std::optional<std::pair<int, int>> expectedCell() const;

        /** @brief Returns the board a chapter opens on: empty, or arranged to illustrate. */
        [[nodiscard]] static hex::HexBoard boardFor(TutorialChapter chapter);

        /** @brief Returns the chapter title. */
        [[nodiscard]] static hexui::StringKey titleFor(TutorialChapter chapter);

        /** @brief Returns the text shown once the chapter awaits no further click. */
        [[nodiscard]] static hexui::StringKey summaryFor(TutorialChapter chapter);

        /** @brief Returns the label of the button leading to the next chapter. */
        [[nodiscard]] static hexui::StringKey nextLabelFor(TutorialChapter chapter);

        /** @brief Returns the colour the player places in this chapter. */
        [[nodiscard]] hex::Player chapterPlayer() const;

        /** @brief Returns the stone the player places in this chapter. */
        [[nodiscard]] hex::Piece chapterPiece() const;

        /** @brief Tests whether the next-chapter button should be shown. */
        [[nodiscard]] bool showsNext() const {
            return finished() && nextAfter(chapter).has_value();
        }

        /** @brief Repositions board and commands within the available area. */
        void layout();

        /** @brief Accepts the click if and only if it lands on the expected cell. */
        void tryPlay(std::pair<int, int> cell);

        /** @brief Advances to the next chapter, if there is one. */
        void advance();
    };
}

#endif //TUTORIAL_STATE_H
