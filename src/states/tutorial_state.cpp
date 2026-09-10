/**
 * @file tutorial_state.cpp
 * @brief Chapter-based onboarding implementation.
 */

#include "states/tutorial_state.h"

#include <string>

#include "core/winning_path.h"
#include "ui/animation.h"
#include "ui/sfml_widgets.h"

namespace hexapp {

    namespace {
        /**
         * @brief Share of the height given to the board.
         * @note The screen splits in two: the top is looked at, the bottom is read.
         * A fraction rather than "everything but a fixed strip", because a fixed
         * strip stops being enough on a short window and lets the title and the Back
         * button slide under the cells.
         */
        constexpr float BOARD_FRACTION = 0.5f;

        /** @brief Margin around the board, in pixels. */
        constexpr float BOARD_MARGIN = 40.0f;

        /** @brief Height of the upper half, the one holding the board. */
        float boardAreaHeight(const float height) { return height * BOARD_FRACTION; }

        /** @brief Builds the board geometry within the upper half of the screen. */
        hexgui::HexLayout layoutFor(const float width, const float height) {
            return hexgui::HexLayout::fit(TUTORIAL_BOARD, width, boardAreaHeight(height),
                                          BOARD_MARGIN);
        }
    }

    const std::vector<TutorialStep>& TutorialState::scriptFor(const TutorialChapter chapter) {
        // The centre column top to bottom: five stones linking Red's top edge to its
        // bottom one. It is the shortest winning game this board admits, and the
        // object of Hex carried out one move at a time.
        //
        // The phrases change three times rather than five: repeating the same
        // instruction on every step is how it stops being read.
        static const std::vector<TutorialStep> red{
            {{0, 2}, hexui::StringKey::TUTORIAL_STEP_GOAL},
            {{1, 2}, hexui::StringKey::TUTORIAL_STEP_CHAIN},
            {{2, 2}, hexui::StringKey::TUTORIAL_STEP_CHAIN},
            {{3, 2}, hexui::StringKey::TUTORIAL_STEP_ALMOST},
            {{4, 2}, hexui::StringKey::TUTORIAL_STEP_ALMOST},
        };

        // The centre row left to right: the same minimal chain rotated ninety
        // degrees. The parallel is deliberate, same length, same phrase rhythm, same
        // position on the board, so that what stands out is the only thing that
        // really differs: the direction the chain grows in.
        static const std::vector<TutorialStep> blue{
            {{2, 0}, hexui::StringKey::TUTORIAL_BLUE_GOAL},
            {{2, 1}, hexui::StringKey::TUTORIAL_BLUE_CHAIN},
            {{2, 2}, hexui::StringKey::TUTORIAL_BLUE_CHAIN},
            {{2, 3}, hexui::StringKey::TUTORIAL_BLUE_ALMOST},
            {{2, 4}, hexui::StringKey::TUTORIAL_BLUE_ALMOST},
        };

        // Explained chapters carry no script: the only thing setting them apart, and
        // enough to make them behave as already finished.
        static const std::vector<TutorialStep> none{};

        switch (chapter) {
            case TutorialChapter::RED_GOAL:  return red;
            case TutorialChapter::BLUE_GOAL: return blue;
            default:                         return none;
        }
    }

    hex::HexBoard TutorialState::boardFor(const TutorialChapter chapter) {
        hex::HexBoard board(TUTORIAL_BOARD);

        switch (chapter) {
            case TutorialChapter::PIE_RULE:
                // A single red stone at the centre: the "first move" the text talks
                // about. Having it in view while reading that Blue may take it makes
                // concrete a rule that sounds like a technicality in words.
                board.addPiece(hex::Piece::RED_DISC, {2, 2});
                break;

            case TutorialChapter::ARCADE:
                // Four walled cells at fixed positions: nothing is drawn at random
                // here. A tutorial must show everyone the same thing, and the text
                // refers to the dimmed cells above it, which a random draw could
                // have placed out of view.
                board.blockCell({0, 1});
                board.blockCell({1, 3});
                board.blockCell({3, 0});
                board.blockCell({4, 2});
                break;

            default:
                break;   // i capitoli giocati partono da una scacchiera vuota
        }

        return board;
    }

    hexui::StringKey TutorialState::titleFor(const TutorialChapter chapter) {
        switch (chapter) {
            case TutorialChapter::BLUE_GOAL: return hexui::StringKey::TUTORIAL_TITLE_BLUE;
            case TutorialChapter::PIE_RULE:  return hexui::StringKey::TUTORIAL_TITLE_PIE;
            case TutorialChapter::ARCADE:    return hexui::StringKey::TUTORIAL_TITLE_ARCADE;
            default:                         return hexui::StringKey::TUTORIAL_TITLE;
        }
    }

    hexui::StringKey TutorialState::summaryFor(const TutorialChapter chapter) {
        // For a played chapter this is the closing line, for an explained one the
        // explanation itself. Either way it is the text to show once no further click
        // is awaited, and the drawing code needs no distinction.
        switch (chapter) {
            case TutorialChapter::BLUE_GOAL: return hexui::StringKey::TUTORIAL_BLUE_DONE;
            case TutorialChapter::PIE_RULE:  return hexui::StringKey::TUTORIAL_PIE_BODY;
            case TutorialChapter::ARCADE:    return hexui::StringKey::TUTORIAL_ARCADE_BODY;
            default:                         return hexui::StringKey::TUTORIAL_DONE;
        }
    }

    hexui::StringKey TutorialState::nextLabelFor(const TutorialChapter chapter) {
        // The label announces where it leads rather than saying Next, so the reader
        // decides whether to continue knowing what comes, and can skip an explained
        // chapter deliberately.
        switch (chapter) {
            case TutorialChapter::RED_GOAL:  return hexui::StringKey::TUTORIAL_NEXT;
            case TutorialChapter::BLUE_GOAL: return hexui::StringKey::TUTORIAL_NEXT_PIE;
            default:                         return hexui::StringKey::TUTORIAL_NEXT_ARCADE;
        }
    }

    std::optional<TutorialChapter> TutorialState::nextAfter(const TutorialChapter chapter) {
        switch (chapter) {
            case TutorialChapter::RED_GOAL:  return TutorialChapter::BLUE_GOAL;
            case TutorialChapter::BLUE_GOAL: return TutorialChapter::PIE_RULE;
            case TutorialChapter::PIE_RULE:  return TutorialChapter::ARCADE;
            default:                         return std::nullopt;   // l'ultimo
        }
    }

    TutorialState::TutorialState(AppContext& context)
        : context(context),
          board(TUTORIAL_BOARD),
          renderer(layoutFor(context.width, context.height), context.assets.font(), context.theme),
          back(context.loc.text(hexui::StringKey::BACK), {}),
          next(context.loc.text(hexui::StringKey::TUTORIAL_NEXT), {}) {
        startChapter(TutorialChapter::RED_GOAL);
    }

    void TutorialState::startChapter(const TutorialChapter chapter) {
        this->chapter = chapter;

        // A fresh board rather than a cleared one: HexBoard also maintains its
        // incremental connectivity, and rebuilding that by hand here would mean
        // knowing a detail that belongs to the board.
        board = boardFor(chapter);

        step = 0;
        elapsed = 0.0f;
        hover.reset();
        winning_path.clear();
        win_elapsed = 0.0f;

        // The button label changes with the chapter: it says where it leads.
        next.setLabel(context.loc.text(nextLabelFor(chapter)));

        layout();
    }

    void TutorialState::advance() {
        if (const std::optional<TutorialChapter> following = nextAfter(chapter)) {
            context.audio.play(hexassets::sounds::CLICK);
            startChapter(*following);
        }
    }

    hex::Player TutorialState::chapterPlayer() const {
        return chapter == TutorialChapter::BLUE_GOAL ? hex::Player::BLUE : hex::Player::RED;
    }

    hex::Piece TutorialState::chapterPiece() const {
        return chapter == TutorialChapter::BLUE_GOAL ? hex::Piece::BLUE_DISC
                                                     : hex::Piece::RED_DISC;
    }

    void TutorialState::layout() {
        renderer.setLayout(layoutFor(context.width, context.height));

        constexpr float BUTTON_W = 240.0f;
        constexpr float BUTTON_H = 42.0f;
        constexpr float BUTTON_GAP = 16.0f;

        const float y = context.height - 66.0f;

        if (showsNext()) {
            // The two commands share the row. Back stays on the left, where it sits
            // when alone, so it is found in the same half of the screen rather than
            // displaced by the arrival of the second button.
            const float total = 2.0f * BUTTON_W + BUTTON_GAP;
            const float x = (context.width - total) / 2.0f;
            back.setBounds({x, y, BUTTON_W, BUTTON_H});
            next.setBounds({x + BUTTON_W + BUTTON_GAP, y, BUTTON_W, BUTTON_H});
        } else {
            back.setBounds({(context.width - BUTTON_W) / 2.0f, y, BUTTON_W, BUTTON_H});
            next.setBounds({});   // nessun riquadro, quindi nessun click
        }
    }

    std::optional<std::pair<int, int>> TutorialState::expectedCell() const {
        if (finished()) return std::nullopt;
        return script()[step].cell;
    }

    void TutorialState::tryPlay(const std::pair<int, int> cell) {
        const std::optional<std::pair<int, int>> wanted = expectedCell();

        // An off-target click is not an error to report: it is a click that is not
        // the right one yet, and the highlighted cell already says so.
        if (!wanted || cell != *wanted) return;

        board.addPiece(chapterPiece(), cell);
        context.audio.play(hexassets::sounds::PLACE);
        ++step;

        if (!finished()) return;

        // The script has closed: from here the command row may hold one more button,
        // so it is re-laid out before the frame draws it.
        layout();

        // The win is recognised by the same function that recognises one in a match:
        // were the script wrong, nothing would appear here.
        if (board.checkWin(chapterPlayer())) {
            winning_path = hexpath::winningPath(board, chapterPlayer());
            win_elapsed = 0.0f;
            context.audio.play(hexassets::sounds::WIN);
        }
    }

    void TutorialState::handleInput(const InputEvent& event) {
        switch (event.type) {
            case InputType::RESIZED:
                layout();
                return;

            case InputType::MOUSE_MOVED:
                hover = renderer.getLayout().cellAt({event.x, event.y});
                back.onMouseMove(event.x, event.y);
                if (showsNext()) next.onMouseMove(event.x, event.y);
                return;

            case InputType::MOUSE_LEFT:
                hover.reset();
                back.onMouseLeave();
                next.onMouseLeave();
                return;

            case InputType::MOUSE_PRESSED: {
                if (event.button != MouseButton::LEFT) return;

                if (showsNext() && next.onMousePress(event.x, event.y)) {
                    advance();
                    return;
                }

                if (back.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    requestTransition(Transition::to(StateId::MAIN_MENU));
                    return;
                }

                if (const std::optional<std::pair<int, int>> cell =
                        renderer.getLayout().cellAt({event.x, event.y})) {
                    tryPlay(*cell);
                }
                return;
            }

            case InputType::KEY_PRESSED:
                // On a finished chapter Enter continues rather than exits: what
                // follows is the next onboarding step, and leaving here would mean
                // never showing the rest.
                if (event.key == Key::ENTER && showsNext()) {
                    advance();
                    return;
                }
                if (event.key == Key::ESCAPE || (finished() && event.key == Key::ENTER)) {
                    requestTransition(Transition::to(StateId::MAIN_MENU));
                }
                return;

            default:
                return;
        }
    }

    void TutorialState::update(const float dt) {
        elapsed += dt;
        if (!winning_path.empty()) win_elapsed += dt;
    }

    void TutorialState::draw(sf::RenderTarget& target) {
        const hexgui::Theme& t = context.theme;
        const sf::Font* font = context.assets.font();

        hexgui::BoardOverlay overlay;
        overlay.winning_path = winning_path;
        overlay.win_elapsed = win_elapsed;

        // While the script is unfinished there is always one cell to touch, and it
        // is the only thing highlighted on the board. Explained chapters have none,
        // and the board stays a still illustration.
        overlay.hint = expectedCell();
        overlay.hint_elapsed = elapsed;

        renderer.draw(target, board, overlay);

        // The preview appears on the target only: showing it on every free cell
        // would suggest they are all playable, which here they are not.
        if (hover && expectedCell() && *hover == *expectedCell()) {
            renderer.drawGhost(target, *hover, chapterPiece(), elapsed);
        }

        // The text starts where the board half ends, so the two parts cannot overlap:
        // they share no pixel.
        const float text_top = boardAreaHeight(context.height) + 16.0f;

        hexgui::drawGlowText(target, context.loc.text(titleFor(chapter)),
                             {0.0f, text_top, context.width, 34 * 1.4f},
                             font, 34, t.hud_primary, t.ui_accent, 5.0f);

        const std::string body = finished()
            ? context.loc.text(summaryFor(chapter))
            : context.loc.text(script()[step].text);

        // The text wraps by itself: the tutorial phrases are long, the pie rule one
        // running to several lines, and a single line would overflow a narrow window.
        hexgui::drawWrappedText(target, body,
                                {context.width * 0.1f, text_top + 56.0f, context.width * 0.8f, 70.0f},
                                font, 18, t.hud_secondary);

        hexgui::drawFlatButton(target, back, font, t, hexgui::ButtonStyle::GHOST,
                               hexgui::Icon::EXIT);

        // Continuing is the primary action of a finished chapter and is drawn as
        // such; otherwise it reads as an accessory next to the exit.
        if (showsNext()) {
            hexgui::drawFlatButton(target, next, font, t, hexgui::ButtonStyle::PRIMARY,
                                   hexgui::Icon::PLAY);
        }

        hexgui::drawCenteredText(target, context.loc.text(hexui::StringKey::TUTORIAL_EXIT),
                                 {0.0f, context.height - 22.0f, context.width, 18.0f},
                                 font, 13, t.label);
    }
}
