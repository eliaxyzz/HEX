/**
 * @file settings_state.cpp
 * @brief Preferences screen implementation.
 */

#include "states/settings_state.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

#include "core/app_preferences.h"
#include "ui/sfml_widgets.h"
#include "ui/window_mode.h"

namespace hexapp {

    namespace {
        /** @brief Width of the widget column. */
        constexpr float COLUMN_W = 420.0f;

        /**
         * @brief Width of the label column, to the left of the controls.
         * @note A pill says what is being chosen between, not what is being chosen.
         * "Off | Low | Medium | High" and "On | Off" are indistinguishable rows
         * without a name in front of them, and read as the same switch repeated. The
         * label is the missing half of the control.
         */
        constexpr float LABEL_W = 170.0f;

        /** @brief Gap between a label and the control it names. */
        constexpr float LABEL_GAP = 16.0f;

        /** @brief Height of one widget row. */
        constexpr float ROW_H = 46.0f;

        /** @brief Vertical gap between consecutive rows. */
        constexpr float GAP = 18.0f;

        /** @brief Height reserved for the note explaining the locks. */
        constexpr float LOCK_NOTE_H = 22.0f;

        /** @brief Strip left clear below the last command. */
        constexpr float BOTTOM_MARGIN = 56.0f;

        /** @brief Option indices; the second is always the enabled one. */
        constexpr int OPTION_OFF = 0;
        constexpr int OPTION_ON = 1;

        /**
         * @brief Indices of the three palettes.
         * @note They coincide with the BoardPalette enumerators, so converting between
         * the two is a cast: the option order is the enum's order, not a second list
         * to keep aligned by hand.
         */
        constexpr int PALETTE_CLASSIC_INDEX = 0;
        constexpr int PALETTE_TOXIC_INDEX = 1;
        constexpr int PALETTE_PRESTIGE_INDEX = 2;

        /** @brief Maps a boolean preference to its option index. */
        int indexOf(const bool on) { return on ? OPTION_ON : OPTION_OFF; }

        /** @brief The four volume steps, as percentages. */
        constexpr std::array<int, 4> MUSIC_STEPS{0, 25, 55, 100};

        /**
         * @brief Returns the step nearest to the stored volume.
         * @note The file may hold any value from 0 to 100, hand-written included, so
         * the nearest step is taken rather than rejecting it: the interface then
         * always shows something sensible.
         */
        int musicStepOf(const int volume) {
            int best = 0;
            for (std::size_t i = 1; i < MUSIC_STEPS.size(); ++i) {
                if (std::abs(MUSIC_STEPS[i] - volume) < std::abs(MUSIC_STEPS[best] - volume)) {
                    best = static_cast<int>(i);
                }
            }
            return best;
        }
    }

    SettingsState::SettingsState(AppContext& context)
        : context(context),
          display({context.loc.text(hexui::StringKey::SETTINGS_WINDOWED),
                   context.loc.text(hexui::StringKey::SETTINGS_FULLSCREEN)},
                  {}, indexOf(context.prefs.fullscreen)),
          audio({context.loc.text(hexui::StringKey::SETTINGS_AUDIO_ON),
                 context.loc.text(hexui::StringKey::SETTINGS_MUTED)},
                {}, indexOf(context.prefs.muted)),
          language({context.loc.text(hexui::StringKey::SETTINGS_LANGUAGE_IT),
                    context.loc.text(hexui::StringKey::SETTINGS_LANGUAGE_EN)},
                   {}, context.prefs.language == hexui::Language::EN ? 1 : 0),
          colours({context.loc.text(hexui::StringKey::SETTINGS_COLOURS_STANDARD),
                   context.loc.text(hexui::StringKey::SETTINGS_COLOURS_COLORBLIND)},
                  {}, indexOf(context.prefs.colorblind)),
          music({context.loc.text(hexui::StringKey::SETTINGS_MUSIC_OFF),
                 context.loc.text(hexui::StringKey::SETTINGS_MUSIC_LOW),
                 context.loc.text(hexui::StringKey::SETTINGS_MUSIC_MEDIUM),
                 context.loc.text(hexui::StringKey::SETTINGS_MUSIC_HIGH)},
                {}, musicStepOf(context.prefs.music_volume)),
          palette({context.loc.text(hexui::StringKey::PALETTE_CLASSIC),
                   context.loc.text(hexui::StringKey::PALETTE_TOXIC),
                   context.loc.text(hexui::StringKey::PALETTE_PRESTIGE)},
                  {}, static_cast<int>(context.prefs.palette)),
          back(context.loc.text(hexui::StringKey::BACK), {}) {
        refreshPaletteLocks();
        layout();
    }

    void SettingsState::refreshPaletteLocks() {
        const int xp = context.profile.xp;

        // Classic is never locked; the other two follow the level. The profile owns
        // the rule, and this only translates it into three booleans.
        palette.setOptionEnabled(PALETTE_CLASSIC_INDEX, true);
        palette.setOptionEnabled(PALETTE_TOXIC_INDEX,
                                 isPaletteUnlocked(BoardPalette::TOXIC, xp));
        palette.setOptionEnabled(PALETTE_PRESTIGE_INDEX,
                                 isPaletteUnlocked(BoardPalette::PRESTIGE, xp));

        // The stored choice may no longer be legitimate: setSelected() refuses a
        // locked one by itself, leaving the group on something valid.
        palette.setSelected(static_cast<int>(usablePalette(context.prefs.palette, xp)));
    }

    void SettingsState::drawPaletteLocks(sf::RenderTarget& target, const sf::Font* font) const {
        const int xp = context.profile.xp;

        // One line per palette still locked, in the order they appear in the
        // selector.
        std::string note;
        const auto mention = [&](const BoardPalette p, const hexui::StringKey name) {
            if (isPaletteUnlocked(p, xp)) return;
            if (!note.empty()) note += "     ";
            note += context.loc.text(name) + " "
                  + context.loc.text(hexui::StringKey::PALETTE_REQUIRES_LEVEL) + " "
                  + std::to_string(requiredLevelFor(p));
        };

        mention(BoardPalette::TOXIC, hexui::StringKey::PALETTE_TOXIC);
        mention(BoardPalette::PRESTIGE, hexui::StringKey::PALETTE_PRESTIGE);

        if (note.empty()) return;   // tutto sbloccato: non c'e' niente da spiegare

        const hexui::Rect& b = palette.bounds();
        hexgui::drawCenteredText(target, note,
                                 {b.x, b.y + b.h + 2.0f, b.w, LOCK_NOTE_H},
                                 font, 14, context.theme.ui_text_muted);
    }

    void SettingsState::applyPalette() {
        context.prefs.palette = static_cast<BoardPalette>(palette.selected());

        // The theme lives in the context and is the one every screen hands to its
        // renderer. Restart from the clean dark theme and retint it, so the tints of
        // one palette do not accumulate on top of another.
        const bool colorblind = context.theme.colorblind;
        context.theme = hexgui::Theme::dark();
        context.theme.colorblind = colorblind;
        hexgui::applyPalette(context.theme, context.prefs.palette);

        persist();
    }

    void SettingsState::layout() {
        // Label and control form a single block; that is what gets centred, not the
        // control column alone, which the labels would otherwise unbalance.
        const float block_w = LABEL_W + LABEL_GAP + COLUMN_W;
        const float x = (context.width - block_w) / 2.0f + LABEL_W + LABEL_GAP;

        // The block is centred in the space below the subtitle rather than starting
        // at a fixed fraction of the height. A fixed fraction pushes the last row
        // further down with every row added, eventually past the bottom edge;
        // centring makes the block reposition itself instead.
        constexpr float ROWS = 6.0f;   // schermo, effetti, lingua, colori, musica, stile
        const float block_h = ROWS * ROW_H + (ROWS - 1.0f) * GAP   // le sei righe
                            + LOCK_NOTE_H                          // la nota dei lucchetti
                            + GAP + ROW_H;                         // stacco e "Indietro"

        const float area_top = context.height * 0.28f;
        const float area_bottom = context.height - BOTTOM_MARGIN;

        float y = area_top + std::max(0.0f, (area_bottom - area_top - block_h) / 2.0f);

        const auto row = [&](const float height) {
            const hexui::Rect r{x, y, COLUMN_W, height};
            y += height + GAP;
            return r;
        };

        display.setBounds(row(ROW_H));
        audio.setBounds(row(ROW_H));
        language.setBounds(row(ROW_H));
        colours.setBounds(row(ROW_H));
        music.setBounds(row(ROW_H));
        palette.setBounds(row(ROW_H));

        // A row is reserved below the palette selector for the lock note, which is
        // where the disabled entries are explained; without reserved space it would
        // land on top of the Back button.
        y += LOCK_NOTE_H;

        y += GAP;
        const hexui::Rect wide = row(ROW_H);
        back.setBounds({wide.x + COLUMN_W / 4.0f, wide.y, COLUMN_W / 2.0f, wide.h});
    }

    void SettingsState::persist() {
        message = savePreferences(context.prefs)
            ? std::string{}
            : context.loc.text(hexui::StringKey::SETTINGS_WRITE_FAILED);
    }

    void SettingsState::applyDisplay() {
        context.prefs.fullscreen = (display.selected() == OPTION_ON);

        // The window is recreated underneath the widgets, so the view size changes
        // and the bounds must be recomputed straight afterwards.
        applyWindowMode(context, context.prefs.fullscreen);
        layout();

        persist();
    }

    void SettingsState::applyAudio() {
        context.prefs.muted = (audio.selected() == OPTION_ON);
        context.audio.setMuted(context.prefs.muted);
        persist();
    }

    void SettingsState::applyColours() {
        context.prefs.colorblind = (colours.selected() == OPTION_ON);

        // The theme lives in the context and is the one every screen hands to its
        // renderer: changing it here suffices, with nobody to notify.
        context.theme.colorblind = context.prefs.colorblind;

        persist();
    }

    void SettingsState::applyMusic() {
        context.prefs.music_volume = MUSIC_STEPS[static_cast<std::size_t>(music.selected())];

        // Immediate effect: a volume is chosen by listening, not by remembering.
        context.audio.setMusicVolume(static_cast<float>(context.prefs.music_volume));

        persist();
    }

    void SettingsState::applyLanguage() {
        context.prefs.language = (language.selected() == OPTION_ON) ? hexui::Language::EN
                                                                    : hexui::Language::IT;
        context.loc.setLanguage(context.prefs.language);

        // Labels already written into the widgets do not change by themselves; they
        // are rewritten, starting with this screen's own.
        const hexui::LocalizationManager& loc = context.loc;
        const int chosen_display = display.selected();
        const int chosen_audio = audio.selected();
        const int chosen_language = language.selected();
        const int chosen_colours = colours.selected();
        const int chosen_music = music.selected();

        display = hexui::OptionGroup({loc.text(hexui::StringKey::SETTINGS_WINDOWED),
                                      loc.text(hexui::StringKey::SETTINGS_FULLSCREEN)},
                                     display.bounds(), chosen_display);
        audio = hexui::OptionGroup({loc.text(hexui::StringKey::SETTINGS_AUDIO_ON),
                                    loc.text(hexui::StringKey::SETTINGS_MUTED)},
                                   audio.bounds(), chosen_audio);
        language = hexui::OptionGroup({loc.text(hexui::StringKey::SETTINGS_LANGUAGE_IT),
                                       loc.text(hexui::StringKey::SETTINGS_LANGUAGE_EN)},
                                      language.bounds(), chosen_language);
        colours = hexui::OptionGroup({loc.text(hexui::StringKey::SETTINGS_COLOURS_STANDARD),
                                      loc.text(hexui::StringKey::SETTINGS_COLOURS_COLORBLIND)},
                                     colours.bounds(), chosen_colours);
        palette = hexui::OptionGroup({loc.text(hexui::StringKey::PALETTE_CLASSIC),
                                      loc.text(hexui::StringKey::PALETTE_TOXIC),
                                      loc.text(hexui::StringKey::PALETTE_PRESTIGE)},
                                     palette.bounds(), palette.selected());
        refreshPaletteLocks();

        music = hexui::OptionGroup({loc.text(hexui::StringKey::SETTINGS_MUSIC_OFF),
                                    loc.text(hexui::StringKey::SETTINGS_MUSIC_LOW),
                                    loc.text(hexui::StringKey::SETTINGS_MUSIC_MEDIUM),
                                    loc.text(hexui::StringKey::SETTINGS_MUSIC_HIGH)},
                                   music.bounds(), chosen_music);
        back.setLabel(loc.text(hexui::StringKey::BACK));

        persist();
    }

    void SettingsState::handleInput(const InputEvent& event) {
        switch (event.type) {
            case InputType::RESIZED:
                layout();
                return;

            case InputType::MOUSE_MOVED:
                display.onMouseMove(event.x, event.y);
                audio.onMouseMove(event.x, event.y);
                language.onMouseMove(event.x, event.y);
                colours.onMouseMove(event.x, event.y);
                music.onMouseMove(event.x, event.y);
                palette.onMouseMove(event.x, event.y);
                back.onMouseMove(event.x, event.y);
                return;

            case InputType::MOUSE_LEFT:
                display.onMouseLeave();
                audio.onMouseLeave();
                language.onMouseLeave();
                colours.onMouseLeave();
                music.onMouseLeave();
                palette.onMouseLeave();
                back.onMouseLeave();
                return;

            case InputType::MOUSE_PRESSED: {
                if (event.button != MouseButton::LEFT) return;

                // Click sound before the effect: muting the audio must still
                // acknowledge the click that muted it.
                if (audio.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    applyAudio();
                    return;
                }
                if (display.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    applyDisplay();
                    return;
                }
                if (language.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    applyLanguage();
                    return;
                }
                if (colours.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    applyColours();
                    return;
                }
                if (music.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    applyMusic();
                    return;
                }
                if (palette.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    applyPalette();
                    return;
                }
                if (back.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    requestTransition(Transition::to(StateId::MAIN_MENU));
                }
                return;
            }

            case InputType::KEY_PRESSED:
                if (event.key == Key::ESCAPE || event.key == Key::ENTER) {
                    requestTransition(Transition::to(StateId::MAIN_MENU));
                }
                return;

            default:
                return;
        }
    }

    void SettingsState::draw(sf::RenderTarget& target) {
        context.backdrop.draw(target, context.width, context.height);

        const hexgui::Theme& t = context.theme;
        const sf::Font* font = context.assets.font();

        const auto line = [&](const std::string& text, const float y, const unsigned size,
                              const sf::Color color) {
            hexgui::drawCenteredText(target, text, {0.0f, y, context.width, size * 1.4f},
                                     font, size, color);
        };

        // The same visual vocabulary as the menu, which is the screen this opens
        // from: two different styles one click apart are noticed.
        hexgui::drawGlowText(target, context.loc.text(hexui::StringKey::SETTINGS_TITLE),
                             {0.0f, context.height * 0.16f, context.width, 52 * 1.4f},
                             font, 52, t.hud_primary, t.ui_accent, 6.0f);
        line(context.loc.text(hexui::StringKey::SETTINGS_SUBTITLE),
             context.height * 0.255f, 18, t.hud_secondary);

        // No icons here: the geometric repertoire has none for fullscreen or for
        // off, and reusing an approximate one, a triangle or three bars, would state
        // something other than what the option does.
        //
        // Each row carries its own name instead. Sound effects and music volume are
        // two different channels, and unnamed the difference has to be guessed.
        const auto labelled = [&](const hexui::OptionGroup& group, const hexui::StringKey key) {
            const hexui::Rect& b = group.bounds();
            hexgui::drawCenteredText(target, context.loc.text(key),
                                     {b.x - LABEL_GAP - LABEL_W, b.y + (b.h - 18 * 1.4f) / 2.0f,
                                      LABEL_W, 18 * 1.4f},
                                     font, 18, t.hud_secondary);
            hexgui::drawSegmented(target, group, font, t);
        };

        labelled(display, hexui::StringKey::SETTINGS_LABEL_DISPLAY);
        labelled(audio, hexui::StringKey::SETTINGS_LABEL_SFX);
        labelled(language, hexui::StringKey::SETTINGS_LABEL_LANGUAGE);
        labelled(colours, hexui::StringKey::SETTINGS_LABEL_COLOURS);
        labelled(music, hexui::StringKey::SETTINGS_MUSIC);
        labelled(palette, hexui::StringKey::SETTINGS_LABEL_PALETTE);
        drawPaletteLocks(target, font);
        hexgui::drawFlatButton(target, back, font, t, hexgui::ButtonStyle::GHOST,
                               hexgui::Icon::EXIT);

        if (!message.empty()) {
            line(message, back.bounds().y + back.bounds().h + GAP, 16, t.ui_accent);
        }
    }
}
