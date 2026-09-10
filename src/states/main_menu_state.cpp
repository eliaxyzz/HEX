/**
 * @file main_menu_state.cpp
 * @brief Main menu implementation: layout, input and rendering.
 */

#include "states/main_menu_state.h"

#include <array>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "ui/animation.h"
#include "core/hex_geometry.h"
#include "core/player_profile.h"
#include "core/save_format.h"
#include "ui/sfml_widgets.h"

namespace hexapp {

    namespace {
        /**
         * @brief Width of the widget column.
         * @note Sized so that a three-way option row still fits an icon and a label
         * inside each pill without clipping the text.
         */
        constexpr float COLUMN_W = 470.0f;

        /** @brief Height of one widget row. */
        constexpr float ROW_H = 44.0f;

        /** @brief Height of the primary action; deliberately dominant. */
        constexpr float PRIMARY_H = 60.0f;

        /** @brief Vertical gap between consecutive rows. */
        constexpr float GAP = 14.0f;

        /** @brief Horizontal gap between the three side-by-side secondary actions. */
        constexpr float SMALL_GAP = 12.0f;

        /** @brief Separation between the choices block and the actions block. */
        constexpr float SECTION_GAP = 26.0f;

        /**
         * @brief Bottom strip reserved for the message and the hint.
         * @warning The widget block must never extend into it; that is what the
         * centring in layout() guarantees.
         */
        constexpr float FOOTER_H = 92.0f;

        /** @brief The three colour choices, in display order. */
        constexpr int SIDE_RED = 0;
        constexpr int SIDE_BLUE = 1;
        constexpr int SIDE_RANDOM = 2;

        /** @brief Indices of the two rule variants. */
        constexpr int VARIANT_NORMAL = 0;
        constexpr int VARIANT_ARCADE = 1;

        /**
         * @brief Tosses a coin between the two colours.
         *
         * Drawn when Play is clicked rather than when the option is selected: an
         * immediate draw would show its result in the menu, turning Random into an
         * awkward way of picking a colour.
         *
         * @note One generator for the whole run. Seeding a fresh one per call would
         * give the same result to two matches started within the same clock tick.
         */
        hex::Player randomColour() {
            static std::mt19937 rng{std::random_device{}()};
            return std::uniform_int_distribution{0, 1}(rng) == 0 ? hex::Player::RED
                                                                 : hex::Player::BLUE;
        }

        /** @brief Duration of the menu intro animation, in seconds. */
        constexpr float INTRO_DURATION = 0.45f;

        /** @brief Initial vertical offset of the intro animation, in pixels. */
        constexpr float INTRO_RISE = 26.0f;

        /** @brief Backdrop scroll speed, in pixels per second. */
        constexpr float BACKDROP_SPEED = 7.0f;

        /** @brief Side length of the decorative backdrop grid. */
        constexpr int BACKDROP_CELLS = 26;

        /**
         * @brief Scoped vertical shift of widget bounds, restored on destruction.
         *
         * The widgets slide upwards on entry, but the offset is for drawing only:
         * bounds must return to where they were so click targets stay still during
         * the animation.
         *
         * @note Restoring in the destructor is what makes that safe. Shifting and
         * restoring by hand needs two lists kept in step, and a widget missing from
         * the second one drifts a little further off screen on every frame.
         */
        template <typename Widget, std::size_t N>
        class BoundsShift {
        public:
            BoundsShift(const std::array<Widget*, N>& widgets, const float dy)
                : items(widgets) {
                for (std::size_t i = 0; i < N; ++i) {
                    saved[i] = items[i]->bounds();

                    hexui::Rect moved = saved[i];
                    moved.y += dy;
                    items[i]->setBounds(moved);
                }
            }

            ~BoundsShift() {
                for (std::size_t i = 0; i < N; ++i) items[i]->setBounds(saved[i]);
            }

            BoundsShift(const BoundsShift&) = delete;
            BoundsShift& operator=(const BoundsShift&) = delete;

        private:
            std::array<Widget*, N> items;
            std::array<hexui::Rect, N> saved{};
        };
    }

    MainMenuState::MainMenuState(AppContext& context)
        : context(context),
          mode({context.loc.text(hexui::StringKey::MENU_MODE_HUMAN_COMPUTER),
                context.loc.text(hexui::StringKey::MENU_MODE_HUMAN_HUMAN)}, {}, 0),
          nickname(context.loc.text(hexui::StringKey::MENU_NAME_1), {}, 16),
          nickname2(context.loc.text(hexui::StringKey::MENU_NAME_2), {}, 16),
          difficulty({context.loc.text(hexui::StringKey::MENU_DIFFICULTY_EASY),
                      context.loc.text(hexui::StringKey::MENU_DIFFICULTY_MEDIUM),
                      context.loc.text(hexui::StringKey::MENU_DIFFICULTY_HARD)}, {}, 1),
          side({context.loc.text(hexui::StringKey::MENU_PLAY_AS_RED),
                context.loc.text(hexui::StringKey::MENU_PLAY_AS_BLUE),
                context.loc.text(hexui::StringKey::MENU_PLAY_AS_RANDOM)}, {}, SIDE_RED),
          variant({context.loc.text(hexui::StringKey::MENU_VARIANT_NORMAL),
                   context.loc.text(hexui::StringKey::MENU_VARIANT_ARCADE)}, {}, VARIANT_NORMAL),
          play(context.loc.text(hexui::StringKey::MENU_PLAY), {}),
          load(context.loc.text(hexui::StringKey::MENU_LOAD), {}),
          online(context.loc.text(hexui::StringKey::MENU_ONLINE), {}),
          settings(context.loc.text(hexui::StringKey::MENU_SETTINGS), {}),
          tutorial(context.loc.text(hexui::StringKey::MENU_TUTORIAL), {}),
          quit(context.loc.text(hexui::StringKey::MENU_QUIT), {}) {
        // The button appears only when there is something to load: opening an empty
        // list is a round trip the interface can spare the user.
        load.setEnabled(!hexsave::listSaves().empty());
        layout();
    }

    void MainMenuState::applyLanguage() {
        const hexui::LocalizationManager& loc = context.loc;

        // An OptionGroup cannot have its labels rewritten one by one, so it is
        // rebuilt while preserving the selection, its only meaningful state.
        const int chosen_mode = mode.selected();
        const int chosen_difficulty = difficulty.selected();
        const int chosen_side = side.selected();
        const int chosen_variant = variant.selected();

        mode = hexui::OptionGroup({loc.text(hexui::StringKey::MENU_MODE_HUMAN_COMPUTER),
                                   loc.text(hexui::StringKey::MENU_MODE_HUMAN_HUMAN)},
                                  mode.bounds(), chosen_mode);
        difficulty = hexui::OptionGroup({loc.text(hexui::StringKey::MENU_DIFFICULTY_EASY),
                                         loc.text(hexui::StringKey::MENU_DIFFICULTY_MEDIUM),
                                         loc.text(hexui::StringKey::MENU_DIFFICULTY_HARD)},
                                        difficulty.bounds(), chosen_difficulty);
        side = hexui::OptionGroup({loc.text(hexui::StringKey::MENU_PLAY_AS_RED),
                                   loc.text(hexui::StringKey::MENU_PLAY_AS_BLUE),
                                   loc.text(hexui::StringKey::MENU_PLAY_AS_RANDOM)},
                                  side.bounds(), chosen_side);
        variant = hexui::OptionGroup({loc.text(hexui::StringKey::MENU_VARIANT_NORMAL),
                                      loc.text(hexui::StringKey::MENU_VARIANT_ARCADE)},
                                     variant.bounds(), chosen_variant);

        play.setLabel(loc.text(hexui::StringKey::MENU_PLAY));
        load.setLabel(loc.text(hexui::StringKey::MENU_LOAD));
        online.setLabel(loc.text(hexui::StringKey::MENU_ONLINE));
        settings.setLabel(loc.text(hexui::StringKey::MENU_SETTINGS));
        tutorial.setLabel(loc.text(hexui::StringKey::MENU_TUTORIAL));
        quit.setLabel(loc.text(hexui::StringKey::MENU_QUIT));
    }

    void MainMenuState::onEnter() {
        entered_since = 0.0f;

        // This screen is re-entered from the settings, where the language may have
        // changed.
        applyLanguage();
        layout();
    }

    void MainMenuState::layout() {
        const float x = (context.width - COLUMN_W) / 2.0f;

        // The block is centred in the space between the subtitle and the footer
        // strip rather than starting at a fixed fraction of the height. A fixed
        // fraction pushes the last button further down with every row added; centring
        // makes the block reposition itself instead.
        constexpr float ROWS_ABOVE = 5.0f;   // modalita', nome, difficolta'/nome2, colore, variante
        const float block_h = ROWS_ABOVE * ROW_H + (ROWS_ABOVE - 1.0f) * GAP   // scelte
                            + SECTION_GAP                                      // stacco
                            + PRIMARY_H + GAP                                  // GIOCA
                            + ROW_H + GAP                                      // riga comandi
                            + ROW_H;                                           // Esci

        const float area_top = context.height * 0.25f;
        const float area_bottom = context.height - FOOTER_H;

        float y = area_top + std::max(0.0f, (area_bottom - area_top - block_h) / 2.0f);

        const auto row = [&](const float height) {
            const hexui::Rect r{x, y, COLUMN_W, height};
            y += height + GAP;
            return r;
        };

        mode.setBounds(row(ROW_H));
        nickname.setBounds(row(ROW_H));

        // Difficulty and the second name share one slot: in whichever mode one of
        // them applies, the other is meaningless.
        const hexui::Rect shared = row(ROW_H);
        difficulty.setBounds(shared);
        nickname2.setBounds(shared);

        // The colour row stays reserved between two humans, where it is not drawn,
        // so the buttons below do not jump when the mode changes.
        side.setBounds(row(ROW_H));

        // The variant closes the choices block: it is the last decision before Play
        // and the only one that applies in both modes, so its row is never hidden.
        variant.setBounds(row(ROW_H));

        y += SECTION_GAP - GAP;
        play.setBounds(row(PRIMARY_H));

        // The three secondary actions share a single row, which is what keeps the
        // block clear of the footer strip.
        const hexui::Rect strip = row(ROW_H);
        const float third = (COLUMN_W - 2.0f * SMALL_GAP) / 3.0f;
        load.setBounds({strip.x, strip.y, third, strip.h});
        online.setBounds({strip.x + third + SMALL_GAP, strip.y, third, strip.h});
        settings.setBounds({strip.x + 2.0f * (third + SMALL_GAP), strip.y, third, strip.h});

        // Tutorial and Quit share the last row: both are secondary actions, and
        // pairing them keeps the block height unchanged.
        const hexui::Rect wide = row(ROW_H);
        const float half = (COLUMN_W - SMALL_GAP) / 2.0f;
        tutorial.setBounds({wide.x, wide.y, half, wide.h});
        quit.setBounds({wide.x + half + SMALL_GAP, wide.y, half, wide.h});
    }

    hexui::TextInput* MainMenuState::focusedField() {
        if (nickname.focused()) return &nickname;
        if (!aiEnabled() && nickname2.focused()) return &nickname2;
        return nullptr;
    }

    float MainMenuState::introOpacity() const {
        return hexanim::easeOutCubic(hexanim::progress(entered_since, INTRO_DURATION));
    }

    float MainMenuState::introOffset() const {
        return INTRO_RISE * (1.0f - introOpacity());
    }

    hex::Player MainMenuState::chosenColour() const {
        switch (side.selected()) {
            case SIDE_BLUE:   return hex::Player::BLUE;
            case SIDE_RANDOM: return randomColour();
            default:          return hex::Player::RED;
        }
    }

    void MainMenuState::startGame() {
        context.settings.mode = aiEnabled() ? GameMode::HUMAN_VS_AI : GameMode::HUMAN_VS_HUMAN;
        context.settings.difficulty = static_cast<Difficulty>(difficulty.selected());
        context.settings.player_name = nickname.text();

        // The second name applies between humans only; against the engine it is
        // derived from the selected level.
        context.settings.opponent_name = aiEnabled() ? std::string{} : nickname2.text();
        context.settings.board_size = 11;

        // Between two humans the colour is not chosen: the first name is Red.
        context.settings.human_colour = aiEnabled() ? chosenColour() : hex::Player::RED;
        context.settings.seconds_per_player = hexplay::DEFAULT_CLOCK_SECONDS;

        // In Arcade the budget above is ignored: clockFor() builds the clock and
        // knows Blitz counts per move rather than per match.
        context.settings.arcade = variant.selected() == VARIANT_ARCADE;
        context.pending_moves.clear();
        context.pending_holes.clear();   // una partita nuova sorteggia i propri

        requestTransition(Transition::to(StateId::PLAYING));
    }

    void MainMenuState::handleInput(const InputEvent& event) {
        switch (event.type) {
            case InputType::RESIZED:
                layout();
                return;

            case InputType::MOUSE_MOVED:
                mode.onMouseMove(event.x, event.y);
                nickname.onMouseMove(event.x, event.y);
                variant.onMouseMove(event.x, event.y);
                if (aiEnabled()) {
                    difficulty.onMouseMove(event.x, event.y);
                    side.onMouseMove(event.x, event.y);
                } else {
                    nickname2.onMouseMove(event.x, event.y);
                }
                play.onMouseMove(event.x, event.y);
                load.onMouseMove(event.x, event.y);
                online.onMouseMove(event.x, event.y);
                settings.onMouseMove(event.x, event.y);
                tutorial.onMouseMove(event.x, event.y);
                quit.onMouseMove(event.x, event.y);
                return;

            case InputType::MOUSE_LEFT:
                mode.onMouseLeave();
                nickname.onMouseLeave();
                nickname2.onMouseLeave();
                difficulty.onMouseLeave();
                side.onMouseLeave();
                variant.onMouseLeave();
                play.onMouseLeave();
                load.onMouseLeave();
                online.onMouseLeave();
                settings.onMouseLeave();
                tutorial.onMouseLeave();
                quit.onMouseLeave();
                return;

            case InputType::MOUSE_PRESSED: {
                if (event.button != MouseButton::LEFT) return;

                if (mode.onMousePress(event.x, event.y))
                    context.audio.play(hexassets::sounds::CLICK);

                // The variant owns its row with no widget stacked on it, so the
                // click reaches it in both modes.
                if (variant.onMousePress(event.x, event.y))
                    context.audio.play(hexassets::sounds::CLICK);

                // Only the visible widget takes the click: the hidden one occupies
                // the same rectangle and would otherwise answer as well.
                if (aiEnabled()) {
                    if (difficulty.onMousePress(event.x, event.y))
                        context.audio.play(hexassets::sounds::CLICK);
                    if (side.onMousePress(event.x, event.y))
                        context.audio.play(hexassets::sounds::CLICK);
                    nickname2.setFocused(false);
                } else {
                    nickname2.onMousePress(event.x, event.y);
                }

                nickname.onMousePress(event.x, event.y);

                if (play.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    startGame();
                } else if (load.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    requestTransition(Transition::to(StateId::LOAD_GAME));
                } else if (online.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    requestTransition(Transition::to(StateId::NETWORK_LOBBY));
                } else if (settings.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    requestTransition(Transition::to(StateId::SETTINGS));
                } else if (tutorial.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    requestTransition(Transition::to(StateId::TUTORIAL));
                } else if (quit.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    requestTransition(Transition::quit());
                }
                return;
            }

            case InputType::TEXT_ENTERED:
                if (hexui::TextInput* field = focusedField()) field->onCharacter(event.text);
                return;

            case InputType::KEY_PRESSED:
                if (event.key == Key::BACKSPACE) {
                    if (hexui::TextInput* field = focusedField()) field->onBackspace();
                    return;
                }
                // With a field focused, Escape leaves the field rather than the
                // application: quitting mid-typing would be a surprise.
                if (event.key == Key::ESCAPE) {
                    if (hexui::TextInput* field = focusedField()) {
                        field->setFocused(false);
                        return;
                    }
                }
                if (event.key == Key::ENTER) {
                    startGame();
                    return;
                }
                if (const std::optional<Transition> t = menuTransitionFor(event)) {
                    requestTransition(*t);
                }
                return;

            default:
                return;
        }
    }

    void MainMenuState::update(const float dt) {
        entered_since += dt;

        nickname.update(dt);
        if (!aiEnabled()) nickname2.update(dt);
    }

    void MainMenuState::draw(sf::RenderTarget& target) {
        const hexgui::Theme& t = context.theme;
        const sf::Font* font = context.assets.font();

        context.backdrop.draw(target, context.width, context.height);

        const float alpha = introOpacity();
        const float rise = introOffset();

        const auto line = [&](const std::string& text, const float y, const unsigned size,
                              const sf::Color color) {
            hexgui::drawCenteredText(target, text, {0.0f, y + rise, context.width, size * 1.4f},
                                     font, size, hexgui::withOpacity(color, alpha));
        };

        // The title carries a glow: the first thing the eye meets should look lit
        // rather than printed.
        hexgui::drawGlowText(target, "HEX",
                             {0.0f, context.height * 0.04f + rise, context.width, 88 * 1.4f},
                             font, 92, hexgui::withOpacity(t.hud_primary, alpha),
                             hexgui::withOpacity(t.ui_accent, alpha * 0.75f), 7.0f);
        line(context.loc.text(hexui::StringKey::MENU_SUBTITLE),
             context.height * 0.205f, 18, t.hud_secondary);

        // The intro offset applies to drawing only; the bounds are restored on the
        // way out of this scope.
        {
            const std::array<hexui::OptionGroup*, 4> groups{&mode, &difficulty, &side, &variant};
            const std::array<hexui::TextInput*, 2> fields{&nickname, &nickname2};
            const std::array<hexui::Button*, 6> buttons{&play, &load, &online, &settings,
                                                        &tutorial, &quit};

            const BoundsShift shifted_groups(groups, rise);
            const BoundsShift shifted_fields(fields, rise);
            const BoundsShift shifted_buttons(buttons, rise);

            using hexgui::Icon;
            using hexgui::ButtonStyle;

            hexgui::drawSegmented(target, mode, font, t,
                                  {Icon::AI, Icon::PEOPLE}, alpha);
            hexgui::drawFlatTextInput(target, nickname, font, t, Icon::USER, alpha);

            if (aiEnabled()) {
                hexgui::drawSegmented(target, difficulty, font, t,
                                      {Icon::LEVEL_1, Icon::LEVEL_2, Icon::LEVEL_3}, alpha);

                // The two stones double as icons, so the colour picked is exactly
                // the one that will appear on the board. The question mark on Random
                // states the only true thing: it is not decided yet.
                hexgui::drawSegmented(target, side, font, t,
                                      {Icon::DISC_RED, Icon::DISC_BLUE, Icon::HELP}, alpha);
            } else {
                hexgui::drawFlatTextInput(target, nickname2, font, t, Icon::USER, alpha);
            }

            // The variant sits outside the branch above: it applies against the
            // engine and between two humans alike, so it is always drawn.
            hexgui::drawSegmented(target, variant, font, t,
                                  {Icon::PLAY, Icon::BLACK_HOLE}, alpha);

            hexgui::drawFlatButton(target, play, font, t, ButtonStyle::PRIMARY, Icon::PLAY, alpha);
            hexgui::drawFlatButton(target, load, font, t, ButtonStyle::SECONDARY, Icon::FOLDER, alpha);
            hexgui::drawFlatButton(target, online, font, t, ButtonStyle::SECONDARY, Icon::ONLINE, alpha);
            hexgui::drawFlatButton(target, settings, font, t, ButtonStyle::SECONDARY, Icon::SETTINGS, alpha);
            hexgui::drawFlatButton(target, tutorial, font, t, ButtonStyle::SECONDARY,
                                   Icon::HELP, alpha);
            hexgui::drawFlatButton(target, quit, font, t, ButtonStyle::GHOST, Icon::EXIT, alpha);
        }

        if (!message.empty()) line(message, context.height - 76.0f, 16, t.ui_accent);
        line(context.loc.text(hexui::StringKey::MENU_HINT), context.height - 44.0f, 15, t.label);

        // The top-right corner reports two different things and they must stay
        // distinguishable: progression only ever rises, the win/loss record does not.
        // Progression goes on top in the accent colour at a larger size, since it
        // changes after every match played; the record sits below in a minor key.
        const int level = levelFor(context.profile.xp);
        const std::string progress = context.loc.text(hexui::StringKey::MENU_LEVEL) + " "
                                   + std::to_string(level) + ": "
                                   + context.loc.text(titleKeyFor(level)) + "   -   "
                                   + std::to_string(context.profile.xp) + " XP";
        hexgui::drawCenteredText(target, progress,
                                 {context.width - 400.0f, 16.0f, 380.0f, 24.0f},
                                 font, 17, hexgui::withOpacity(t.ui_accent, alpha));

        // The record against the engine: a figure that accompanies, not one to put
        // in the way of someone who only wants to play.
        const std::string record = context.loc.text(hexui::StringKey::MENU_WINS) + ": "
                                 + std::to_string(context.prefs.wins) + "   "
                                 + context.loc.text(hexui::StringKey::MENU_LOSSES) + ": "
                                 + std::to_string(context.prefs.losses);
        hexgui::drawCenteredText(target, record,
                                 {context.width - 400.0f, 42.0f, 380.0f, 22.0f},
                                 font, 15, hexgui::withOpacity(t.hud_secondary, alpha));
    }
}
