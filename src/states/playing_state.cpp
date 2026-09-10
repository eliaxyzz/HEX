/**
 * @file playing_state.cpp
 * @brief Local match screen implementation.
 */

#include "states/playing_state.h"

#include <algorithm>
#include <array>
#include <random>
#include <vector>

#include "ui/animation.h"
#include "core/app_preferences.h"
#include "core/arcade.h"
#include "core/player_profile.h"
#include "core/save_format.h"
#include "ui/sfml_widgets.h"
#include "core/winning_path.h"

namespace hexapp {

    namespace {
        /** @brief Bottom strip reserved for the status bar, clocks and commands. */
        constexpr float HUD_HEIGHT = 150.0f;

        /** @brief Height of the command bar buttons. */
        constexpr float BUTTON_H = 36.0f;

        /** @brief Gap between consecutive buttons. */
        constexpr float BUTTON_GAP = 10.0f;

        /** @brief Margin between the buttons and the window edges. */
        constexpr float BUTTON_MARGIN = 24.0f;

        /** @brief Side margin of the status bar text. */
        constexpr float HUD_MARGIN = 24.0f;

        /** @brief Margin around the board, leaving room for the edge labels. */
        constexpr float BOARD_MARGIN = 56.0f;

        /** @brief Width of the save name prompt. */
        constexpr float PROMPT_W = 460.0f;

        /** @brief Height of the save name prompt. */
        constexpr float PROMPT_H = 210.0f;

        /** @brief Builds a board layout that keeps the status strip clear. */
        hexgui::HexLayout layoutFor(const int size, const float width, const float height) {
            return hexgui::HexLayout::fit(size, width, height - HUD_HEIGHT, BOARD_MARGIN);
        }

        /**
         * @brief Builds the player holding the given colour.
         * @note The settings decide who holds a colour, not the turn order: picking
         * Blue makes Red the engine, and the match opens by itself with the engine's
         * move.
         */
        std::unique_ptr<hex::AbstractPlayer> makePlayer(const GameSettings& s,
                                                        const hex::Player colour,
                                                        const hexui::LocalizationManager& loc) {
            if (isHumanColour(s, colour))
                return std::make_unique<hexgui::SfmlHumanPlayer>(nameOfColour(s, colour, loc));

            return std::make_unique<hex::HexPlayer>(configFor(s.difficulty));
        }

        /**
         * @brief Builds the board the match starts from.
         *
         * In Arcade the black holes are walled off here, before the controller
         * exists: the only point at which "before the first move" holds by
         * construction rather than by caller discipline. Once the match is up there
         * is no way to wall a cell, and there must not be one.
         *
         * @note Seeded from the system clock, so two consecutive Arcade matches get
         * different holes; otherwise the variety the mode promises lasts one match.
         */
        hex::HexBoard boardFor(const GameSettings& s,
                               const std::vector<std::pair<int, int>>& saved_holes) {
            hex::HexBoard board(s.board_size);

            // A loaded match reopens its own holes, not four fresh ones: the moves
            // already played only make sense on that board. The draw therefore
            // applies to a new match only.
            if (!saved_holes.empty()) {
                for (const std::pair<int, int>& h : saved_holes) board.blockCell(h);
                return board;
            }

            if (!s.arcade) return board;

            std::mt19937 rng{std::random_device{}()};
            (void)hexplay::applyBlackHoles(board, hexplay::ARCADE_BLACK_HOLES, rng);
            return board;
        }
    }

    PlayingState::PlayingState(AppContext& context)
        : context(context),
          red(makePlayer(context.settings, hex::Player::RED, context.loc)),
          blue(makePlayer(context.settings, hex::Player::BLUE, context.loc)),
          controller(boardFor(context.settings, context.pending_holes), *red, *blue),
          observer(context.loc),
          renderer(layoutFor(context.settings.board_size, context.width, context.height),
                   context.assets.font(), context.theme),
          clock(clockFor(context.settings)),
          swap_button(context.loc.text(hexui::StringKey::GAME_PIE_RULE), {}),
          new_game_button(context.loc.text(hexui::StringKey::GAME_NEW_GAME), {}),
          leave_button(context.loc.text(hexui::StringKey::GAME_LEAVE), {}),
          save_button(context.loc.text(hexui::StringKey::GAME_SAVE), {}),
          save_name(context.loc.text(hexui::StringKey::GAME_SAVE_NAME), {},
                    hexsave::MAX_SLOT_LENGTH),
          save_confirm(context.loc.text(hexui::StringKey::GAME_SAVE_CONFIRM), {}),
          save_cancel(context.loc.text(hexui::StringKey::GAME_CANCEL), {}),
          leave_dialog(context, hexui::StringKey::GAME_LEAVE_PROMPT,
                       hexui::StringKey::GAME_LEAVE_HINT, hexui::StringKey::GAME_LEAVE) {

        // The settings say which colours are human, and they are the same ones that
        // just built the players: the cast is safe because the question asked here is
        // the one that decided the type.
        if (isHumanColour(context.settings, hex::Player::RED))
            human_red = static_cast<hexgui::SfmlHumanPlayer*>(red.get());
        if (isHumanColour(context.settings, hex::Player::BLUE))
            human_blue = static_cast<hexgui::SfmlHumanPlayer*>(blue.get());

        // A loaded match is rebuilt by replaying its moves, before anything observes
        // it. A failed replay leaves the initial position: a fresh match beats an
        // inconsistent one.
        if (!context.pending_moves.empty()) {
            if (!controller.replay(context.pending_moves))
                save_message = context.loc.text(hexui::StringKey::GAME_REPLAY_FAILED);
            context.pending_moves.clear();
        }

        // Consumed here: like the moves, these belong to this match and not to the
        // next, which would otherwise reopen the previous match's holes.
        context.pending_holes.clear();

        controller.addObserver(observer);
        layoutButtons();
        refreshCommands();
    }

    std::vector<hexui::Button*> PlayingState::commandButtons() {
        // The bar lists what can be done now, not the game's full command inventory:
        // the swap exists only on the turn where it is legal, and leaving and
        // restarting exclude each other.
        std::vector<hexui::Button*> buttons;
        buttons.reserve(3);
        if (swap_visible) buttons.push_back(&swap_button);
        buttons.push_back(over_visible ? &new_game_button : &leave_button);
        buttons.push_back(&save_button);
        return buttons;
    }

    void PlayingState::layoutButtons() {
        // Fixed widths, measured per label: a button that changes width from one
        // turn to the next makes the whole bar shift.
        constexpr float SWAP_W = 110.0f;
        constexpr float NEW_GAME_W = 150.0f;
        constexpr float LEAVE_W = 130.0f;
        constexpr float SAVE_W = 90.0f;

        std::vector<float> widths;
        widths.reserve(3);
        if (swap_visible) widths.push_back(SWAP_W);
        widths.push_back(over_visible ? NEW_GAME_W : LEAVE_W);
        widths.push_back(SAVE_W);

        const std::vector<hexui::Button*> buttons = commandButtons();

        float total = 0.0f;
        for (const float w : widths) total += w + BUTTON_GAP;
        total -= BUTTON_GAP;

        // Right-aligned but never past the window edge: on a narrow window the
        // commands stay reachable, at worst covering the hint.
        const float y = context.height - BUTTON_H - 20.0f;
        float x = std::max(HUD_MARGIN, context.width - BUTTON_MARGIN - total);

        for (std::size_t i = 0; i < buttons.size(); ++i) {
            buttons[i]->setBounds({x, y, widths[i], BUTTON_H});
            x += widths[i] + BUTTON_GAP;
        }

        // The name prompt is centred on screen: being modal, it has no place of its
        // own in the command bar.
        const float px = (context.width - PROMPT_W) / 2.0f;
        const float py = (context.height - PROMPT_H) / 2.0f;

        save_name.setBounds({px + 24.0f, py + 74.0f, PROMPT_W - 48.0f, 44.0f});

        constexpr float ACTION_W = 150.0f;
        const float action_y = py + PROMPT_H - 58.0f;
        save_cancel.setBounds({px + 24.0f, action_y, ACTION_W, 42.0f});
        save_confirm.setBounds({px + PROMPT_W - 24.0f - ACTION_W, action_y, ACTION_W, 42.0f});

        leave_dialog.layout();
    }

    void PlayingState::refreshCommands() {
        const hexgui::SfmlHumanPlayer* human = armedHuman();

        // The swap is legal only on the turn where the engine generated it among the
        // valid moves, and only for a human. Outside that the command does not exist:
        // it leaves the bar rather than sitting grey for the rest of the match, since
        // a disabled button keeps promising something.
        const bool can_swap = human != nullptr && human->canSwap();
        swap_button.setEnabled(can_swap);

        // New game takes the place of Leave once there is nothing left to leave, and
        // that is where it is wanted: a rematch is asked for while looking at the
        // result, not after a detour through the menu.
        const bool is_over = controller.isOver();

        if (can_swap != swap_visible || is_over != over_visible) {
            swap_visible = can_swap;
            over_visible = is_over;

            // The bar changed composition: the commands slide to take up the space
            // that was freed.
            layoutButtons();
        }
    }

    void PlayingState::startRematch() {
        // Colours alternate: whoever opened the match just finished now responds.
        // The first-move advantage is real, and handing it to the same side every
        // time would turn a series of matches into one match repeated.
        context.settings = rematchOf(context.settings);

        // The rematch is a new match, not this one wiped clean: no moves to replay,
        // and the screen is rebuilt from scratch.
        context.pending_moves.clear();

        requestTransition(Transition::to(StateId::PLAYING));
    }

    void PlayingState::tickClock(const float dt) {
        // Time runs only for the side to move, and only while the match is live:
        // sitting in front of a finished game is nobody's time.
        clock.setRunning(controller.isOver()
                             ? std::nullopt
                             : std::optional{controller.situation().toMove()});
        clock.tick(dt);

        if (const std::optional<hex::Player> loser = clock.expiredPlayer()) {
            // The controller declares the loss: the clock reports, it does not
            // referee. The outcome therefore travels the same path as a per-move
            // timeout, observers included.
            (void)controller.claimTimeout(*loser);
        }
    }

    void PlayingState::recordOutcome() {
        if (outcome_recorded || !controller.isOver()) return;
        if (context.settings.mode != GameMode::HUMAN_VS_AI) return;

        const std::optional<hex::GameResult> outcome = controller.result();
        if (!outcome) return;

        outcome_recorded = true;

        const bool human_won = outcome->winner == context.settings.human_colour;

        if (human_won) ++context.prefs.wins;
        else           ++context.prefs.losses;

        // Experience is granted here because this function already carries the two
        // guarantees it needs: it runs exactly once per match (outcome_recorded) and
        // only once the match is over (controller.isOver()). Abandoning midway leaves
        // the screen without ever reaching this point, which is how an unfinished
        // match earns nothing: by construction, not through an extra check somebody
        // could forget to update.
        //
        // A timeout is not a special case: it is a loss like any other, and why the
        // match ended does not change what was learned playing it.
        context.profile = withXpGain(context.profile, human_won ? XP_FOR_WIN : XP_FOR_LOSS);

        // Neither the tally nor the experience survives shutdown unless written. A
        // failed write stops nothing: the match matters more than the counter.
        (void)savePreferences(context.prefs);
        (void)saveProfile(context.profile);
    }

    void PlayingState::openSavePrompt() {
        save_prompt = true;
        save_message.clear();
        save_name.clear();
        save_name.setFocused(true);
    }

    void PlayingState::closeSavePrompt() {
        save_prompt = false;
        save_name.setFocused(false);
    }

    void PlayingState::confirmSave() {
        const std::optional<std::string> slot = hexsave::normaliseSlotName(save_name.text());
        if (!slot) {
            // The prompt stays open with the explanation: closing here would discard
            // the typed name along with the save.
            save_message = context.loc.text(hexui::StringKey::GAME_SAVE_BAD_NAME);
            return;
        }

        saveGame(*slot);
        closeSavePrompt();
    }

    bool PlayingState::handleSavePromptInput(const InputEvent& event) {
        if (!save_prompt) return false;

        // Genuinely modal: while open, the match below receives nothing. Otherwise
        // Escape would exit to the menu mid-name, and a click would play a move
        // through the panel onto the board.
        switch (event.type) {
            case InputType::MOUSE_MOVED:
                save_name.onMouseMove(event.x, event.y);
                save_confirm.onMouseMove(event.x, event.y);
                save_cancel.onMouseMove(event.x, event.y);
                return true;

            case InputType::MOUSE_LEFT:
                save_name.onMouseLeave();
                save_confirm.onMouseLeave();
                save_cancel.onMouseLeave();
                return true;

            case InputType::MOUSE_PRESSED:
                if (event.button != MouseButton::LEFT) return true;

                if (save_confirm.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    confirmSave();
                    return true;
                }
                if (save_cancel.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    closeSavePrompt();
                    return true;
                }

                // The field regains focus on every click: inside the prompt it is the
                // only thing that can be typed into.
                save_name.onMousePress(event.x, event.y);
                save_name.setFocused(true);
                return true;

            case InputType::TEXT_ENTERED:
                (void)save_name.onCharacter(event.text);
                return true;

            case InputType::KEY_PRESSED:
                if (event.key == Key::BACKSPACE)   (void)save_name.onBackspace();
                else if (event.key == Key::ENTER)  confirmSave();
                else if (event.key == Key::ESCAPE) closeSavePrompt();
                return true;

            case InputType::RESIZED:
                return false;   // il ridimensionamento riguarda tutta la schermata

            default:
                return true;
        }
    }

    void PlayingState::saveGame(const std::string& slot) {
        hexsave::SaveData data;
        data.board_size = controller.situation().getBoard().getSize();
        data.mode = context.settings.mode;
        data.difficulty = context.settings.difficulty;

        // Who was playing, not only what was played: that is what lets a reloaded
        // match know whose the two names are, and a rematch swap the right roles.
        data.human_colour = context.settings.human_colour;
        data.red_name = firstPlayerName(context.settings, context.loc);
        data.blue_name = secondPlayerName(context.settings, context.loc);
        data.moves = hexsave::tokensFromMoves(controller.moveHistory());

        // The black holes are read back from the board rather than from the
        // settings: on the board they actually exist, and they are the real ones. The
        // settings only know the match is Arcade, not where the draw put them.
        data.holes = controller.situation().getBoard().getPosByPiece(hex::Piece::BLOCKED);

        std::string error;
        save_message = hexsave::writeToFile(slot, data, error)
            ? context.loc.text(hexui::StringKey::GAME_SAVED) + " \"" + slot + "\" ("
                  + std::to_string(data.moves.size()) + " "
                  + context.loc.text(hexui::StringKey::GAME_MOVES) + ")"
            : context.loc.text(hexui::StringKey::GAME_SAVE_FAILED) + ": " + error;
    }

    std::optional<std::pair<int, int>> PlayingState::highlightCell() const {
        if (observer.lastMove()) return observer.lastMove();

        // After a load the observer has seen no move go by.
        for (auto it = controller.moveHistory().rbegin(); it != controller.moveHistory().rend(); ++it) {
            if (it->kind == hex::MoveKind::ADD) return it->action.position;
        }
        return std::nullopt;
    }

    bool PlayingState::isHuman(const hex::Player p) const {
        return p == hex::Player::RED ? human_red != nullptr : human_blue != nullptr;
    }

    hexgui::SfmlHumanPlayer* PlayingState::armedHuman() {
        if (human_red && human_red->isArmed()) return human_red;
        if (human_blue && human_blue->isArmed()) return human_blue;
        return nullptr;
    }

    void PlayingState::relayout(const float width, const float height) {
        renderer.setLayout(layoutFor(controller.situation().getBoard().getSize(), width, height));
    }

    void PlayingState::handleInput(const InputEvent& event) {
        // Dialogs come first: they are modal, so while one is open no other command
        // on this screen exists.
        if (handleSavePromptInput(event)) return;

        switch (leave_dialog.handleInput(event)) {
            case DialogOutcome::CONFIRMED:
                requestTransition(Transition::to(StateId::MAIN_MENU));
                return;
            case DialogOutcome::CONSUMED:
            case DialogOutcome::CANCELLED:
                return;
            case DialogOutcome::IGNORED:
                break;
        }

        // Escape during a running match asks for confirmation instead of leaving.
        // The navigation rule still applies, but only once there is no match left to
        // abandon.
        if (event.type == InputType::KEY_PRESSED && event.key == Key::ESCAPE
            && !controller.isOver()) {
            leave_dialog.open();
            return;
        }

        // Then navigation: the rule lives in app_state.cpp and is covered by tests.
        if (const std::optional<Transition> t = playingTransitionFor(event)) {
            requestTransition(*t);
            return;
        }

        switch (event.type) {
            case InputType::RESIZED:
                relayout(event.x, event.y);
                layoutButtons();
                break;

            case InputType::MOUSE_MOVED:
                if (const std::optional<std::pair<int, int>> under =
                        renderer.getLayout().cellAt({event.x, event.y});
                    under != hover) {
                    hover = under;
                    hover_elapsed = 0.0f;
                }
                for (hexui::Button* b : commandButtons()) b->onMouseMove(event.x, event.y);
                break;

            case InputType::MOUSE_LEFT:
                hover.reset();
                for (hexui::Button* b : commandButtons()) b->onMouseLeave();
                break;

            case InputType::MOUSE_PRESSED: {
                if (event.button != MouseButton::LEFT) break;

                // Commands take precedence over the board. They do not overlap as
                // laid out, but the rule holds if they ever do. The swap is consulted
                // only while present: when it is not, its rectangle belongs to
                // whichever command slid into its place.
                if (swap_visible && swap_button.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    if (hexgui::SfmlHumanPlayer* player = armedHuman()) player->onSwapRequested();
                    break;
                }
                if (over_visible) {
                    if (new_game_button.onMousePress(event.x, event.y)) {
                        context.audio.play(hexassets::sounds::CLICK);
                        startRematch();
                        break;
                    }
                } else if (leave_button.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    leave_dialog.open();
                    break;
                }
                if (save_button.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    openSavePrompt();
                    break;
                }

                const auto cell = renderer.getLayout().cellAt({event.x, event.y});
                if (!cell) break;   // click fuori dalla scacchiera

                // The click goes to whoever was asked; it validates itself, so an
                // illegal click produces nothing.
                if (hexgui::SfmlHumanPlayer* player = armedHuman()) player->onCellClicked(*cell);
                break;
            }

            case InputType::KEY_PRESSED:
                if (event.key == Key::S) {
                    if (hexgui::SfmlHumanPlayer* player = armedHuman()) player->onSwapRequested();
                } else if (event.key == Key::R) {
                    openSavePrompt();
                }
                break;

            default:
                break;
        }
    }

    void PlayingState::syncAnimation() {
        const std::size_t now = controller.moveCount();

        // Only one additional move deserves an animation. An undo lowers the count
        // and a load raises it by many: showing a stone appear would mislead in both
        // cases.
        if (now == seen_moves + 1) {
            const hex::Move& m = controller.moveHistory().back();
            if (m.kind == hex::MoveKind::ADD) {
                appearing = m.action.position;
                appear_elapsed = 0.0f;

                // The sound accompanies the animation, so it starts where the
                // animation does: once per new move, never on a replay or a reset.
                context.audio.play(hexassets::sounds::PLACE);
            } else {
                appearing.reset();   // lo scambio non fa comparire nulla
            }
        } else if (now != seen_moves) {
            appearing.reset();
        }

        seen_moves = now;
    }

    void PlayingState::syncWinningPath() {
        // A win by resignation or timeout has no chain to show.
        const std::optional<hex::GameResult> result = controller.result();
        const bool connected = result && result->reason == hex::EndReason::CONNECTION;

        if (!connected) {
            winning_path.clear();
            win_elapsed = 0.0f;
            return;
        }
        if (winning_path.empty()) {
            winning_path = hexpath::winningPath(controller.situation().getBoard(), result->winner);
            win_elapsed = 0.0f;   // il bagliore parte dal primo anello della catena

            // The chain appears once per match, at the moment the result becomes
            // visible, which is where the sound belongs.
            if (!winning_path.empty()) context.audio.play(hexassets::sounds::WIN);
        }
    }

    void PlayingState::update(const float dt) {
        // The caret blinks only where typing is happening.
        if (save_prompt) save_name.update(dt);

        appear_elapsed += dt;
        hover_elapsed += dt;
        if (!winning_path.empty()) win_elapsed += dt;

        // One step per frame, never blocking: if the side to move has not decided
        // yet it returns WAITING and the frame carries on.
        last_step = controller.step();

        tickClock(dt);
        syncAnimation();
        syncWinningPath();
        recordOutcome();
        refreshCommands();
    }

    void PlayingState::draw(sf::RenderTarget& target) {
        hexgui::BoardOverlay overlay;
        overlay.last_move = highlightCell();
        overlay.winning_path = winning_path;
        overlay.win_elapsed = win_elapsed;

        if (appearing) {
            const float t = hexanim::progress(appear_elapsed, hexanim::PLACEMENT_DURATION);
            overlay.appearing = appearing;
            overlay.appear_progress = hexanim::easeOutCubic(t);
        }

        renderer.draw(target, controller.situation().getBoard(), overlay);

        // Move preview, shown only where a click would actually be accepted.
        if (const hexgui::SfmlHumanPlayer* player = armedHuman();
            player && hover && player->wouldAccept(*hover)) {
            renderer.drawGhost(target, *hover, hex::pieceOf(player->askedSituation().toMove()),
                               hover_elapsed);
        }

        std::string status = observer.statusText();
        if (last_step == hex::StepResult::WAITING && !controller.isOver()
            && !isHuman(controller.situation().toMove())) {
            status += "  -  " + context.loc.text(hexui::StringKey::GAME_THINKING);
        }

        // The clocks. Blitz shows one rather than two: the opponent's time is not a
        // reserve sitting there waiting, so displaying it would misreport the state.
        // Only what is left to the side to move counts, and it is read beside that
        // player's name, where the eye is already looking.
        std::string clocks;
        if (clock.enabled() && clock.mode() == hexplay::ClockMode::PER_TURN) {
            if (!controller.isOver()) {
                status += "  -  "
                        + hexplay::GameClock::formatTenths(
                              clock.remaining(controller.situation().toMove()));
            }
            clocks = context.loc.text(hexui::StringKey::ARCADE_BLITZ);
        }
        else if (clock.enabled()) {
            const bool red_runs = !controller.isOver()
                                  && controller.situation().toMove() == hex::Player::RED;

            clocks = std::string(red_runs ? "> " : "  ")
                   + context.loc.text(hexui::StringKey::COLOUR_RED) + " "
                   + hexplay::GameClock::format(clock.remaining(hex::Player::RED))
                   + "     " + (red_runs ? "  " : "> ")
                   + context.loc.text(hexui::StringKey::COLOUR_BLUE) + " "
                   + hexplay::GameClock::format(clock.remaining(hex::Player::BLUE));
        }

        const std::string notice = save_message.empty() ? observer.noticeText() : save_message;

        const std::vector<std::string> hud{status, clocks, notice};
        renderer.drawHud(target, hud, {HUD_MARGIN, context.height - HUD_HEIGHT + 12.0f});

        // The commands live in the buttons; the keyboard keeps only the exit, the
        // one shortcut a user expects to find without being told.
        const hexui::Rect hint_area{HUD_MARGIN, context.height - BUTTON_H - 20.0f,
                                    200.0f, BUTTON_H};
        // On a finished match Escape really leaves; on a running one it asks first,
        // and the hint says which of the two it will do.
        hexgui::drawCenteredText(target,
                                 context.loc.text(over_visible ? hexui::StringKey::GAME_MENU_HINT_OVER
                                                               : hexui::StringKey::GAME_MENU_HINT),
                                 hint_area, context.assets.font(), 15,
                                 context.theme.hud_secondary);

        for (const hexui::Button* b : commandButtons())
            hexgui::drawButton(target, *b, context.assets, context.theme);

        if (save_prompt) drawSavePrompt(target);
        leave_dialog.draw(target);
    }

    void PlayingState::drawSavePrompt(sf::RenderTarget& target) {
        const hexgui::Theme& t = context.theme;
        const sf::Font* font = context.assets.font();

        // The dark veil is not decoration: it signals that the match below has
        // stopped listening, and is what makes the prompt read as modal.
        sf::RectangleShape veil({context.width, context.height});
        veil.setFillColor(sf::Color(0, 0, 0, 150));
        target.draw(veil);

        const float px = (context.width - PROMPT_W) / 2.0f;
        const float py = (context.height - PROMPT_H) / 2.0f;

        hexgui::drawRoundedRect(target, {px, py, PROMPT_W, PROMPT_H}, 14.0f,
                                t.ui_surface, t.ui_border, 2.0f);

        hexgui::drawCenteredText(target, context.loc.text(hexui::StringKey::GAME_SAVE_PROMPT),
                                 {px, py + 24.0f, PROMPT_W, 22 * 1.4f}, font, 22, t.hud_primary);

        hexgui::drawFlatTextInput(target, save_name, font, t, hexgui::Icon::FOLDER);

        hexgui::drawFlatButton(target, save_cancel, font, t, hexgui::ButtonStyle::GHOST);
        hexgui::drawFlatButton(target, save_confirm, font, t, hexgui::ButtonStyle::PRIMARY);

        // Below the field: either why a name was rejected, or how to confirm.
        const bool bad_name = save_message == context.loc.text(hexui::StringKey::GAME_SAVE_BAD_NAME);
        hexgui::drawCenteredText(target,
                                 bad_name ? save_message
                                          : context.loc.text(hexui::StringKey::GAME_SAVE_HINT),
                                 {px, py + 128.0f, PROMPT_W, 15 * 1.4f}, font, 15,
                                 bad_name ? t.ui_accent : t.label);
    }
}
