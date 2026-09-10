/**
 * @file load_game_state.cpp
 * @brief Save selection screen implementation.
 */

#include "states/load_game_state.h"

#include <algorithm>
#include <optional>

#include "core/save_format.h"
#include "ui/sfml_widgets.h"

namespace hexapp {

    namespace {
        /** @brief Width of the list column. */
        constexpr float COLUMN_W = 460.0f;

        /** @brief Height of one list row. */
        constexpr float ROW_H = 46.0f;

        /** @brief Vertical gap between consecutive rows. */
        constexpr float GAP = 12.0f;

        /** @brief Separation between the list and the service commands. */
        constexpr float SECTION_GAP = 24.0f;

        /**
         * @brief Side length of the two per-row commands.
         * @note Square and narrow: they carry an icon and no label, and must take as
         * little of the row as possible. The save name is the row's information;
         * rename and delete are accessories and look like it.
         */
        constexpr float ACTION_SIDE = 40.0f;

        /** @brief Gap between the name and the commands, and between the commands. */
        constexpr float ACTION_GAP = 8.0f;

        /** @brief Width of the rename panel. */
        constexpr float PROMPT_W = 460.0f;

        /** @brief Height of the rename panel. */
        constexpr float PROMPT_H = 210.0f;
    }

    LoadGameState::LoadGameState(AppContext& context)
        : context(context),
          saves(hexsave::listSaves()),
          prev(context.loc.text(hexui::StringKey::MENU_LOAD_PREV), {}),
          next(context.loc.text(hexui::StringKey::MENU_LOAD_NEXT), {}),
          back(context.loc.text(hexui::StringKey::BACK), {}),
          delete_dialog(context, hexui::StringKey::SAVE_DELETE_PROMPT,
                        hexui::StringKey::SAVE_MANAGE_HINT,
                        hexui::StringKey::SAVE_DELETE_CONFIRM, hexgui::Icon::TRASH),
          rename_name(context.loc.text(hexui::StringKey::GAME_SAVE_NAME), {},
                      hexsave::MAX_SLOT_LENGTH),
          rename_confirm(context.loc.text(hexui::StringKey::SAVE_RENAME_CONFIRM), {}),
          rename_cancel(context.loc.text(hexui::StringKey::GAME_CANCEL), {}) {
        entries.reserve(SAVES_PER_PAGE);
        renames.reserve(SAVES_PER_PAGE);
        deletes.reserve(SAVES_PER_PAGE);
        for (std::size_t i = 0; i < SAVES_PER_PAGE; ++i) {
            entries.emplace_back(std::string{}, hexui::Rect{});
            renames.emplace_back(std::string{}, hexui::Rect{});
            deletes.emplace_back(std::string{}, hexui::Rect{});
        }

        layout();
    }

    std::size_t LoadGameState::visibleCount() const {
        return std::min(SAVES_PER_PAGE, saves.size() - std::min(first, saves.size()));
    }

    void LoadGameState::layout() {
        const float x = (context.width - COLUMN_W) / 2.0f;

        // The block stays centred whatever the row count: a page holding two saves
        // must not look like a six-row page left half empty.
        const std::size_t rows = std::max<std::size_t>(visibleCount(), 1);
        const float list_h = static_cast<float>(rows) * ROW_H + (rows - 1) * GAP;
        const float block_h = list_h + SECTION_GAP + ROW_H;

        float y = std::max(context.height * 0.30f,
                           (context.height - block_h) / 2.0f);

        // The name yields the right of the row to the two commands. The subtraction
        // lives here once, so changing the command size shortens the name instead of
        // letting it slide underneath.
        const float name_w = COLUMN_W - 2.0f * (ACTION_SIDE + ACTION_GAP);

        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (i >= visibleCount()) {
                // Off the page: no bounds, therefore no click.
                entries[i].setBounds({});
                entries[i].setLabel({});
                renames[i].setBounds({});
                deletes[i].setBounds({});
                continue;
            }

            entries[i].setLabel(saves[first + i]);
            entries[i].setBounds({x, y, name_w, ROW_H});

            // Rename first, delete last: the destructive one sits at the far end,
            // where the pointer does not land while aiming for the other.
            renames[i].setBounds({x + name_w + ACTION_GAP, y, ACTION_SIDE, ROW_H});
            deletes[i].setBounds({x + name_w + 2.0f * ACTION_GAP + ACTION_SIDE, y,
                                  ACTION_SIDE, ROW_H});
            y += ROW_H + GAP;
        }

        y = (y - GAP) + SECTION_GAP;

        // The paging commands exist only when there is somewhere to go: a Next
        // button with no next page breaks the same promise a greyed button does.
        const float third = (COLUMN_W - 2.0f * GAP) / 3.0f;
        prev.setBounds(hasPrev() ? hexui::Rect{x, y, third, ROW_H} : hexui::Rect{});
        next.setBounds(hasNext() ? hexui::Rect{x + 2.0f * (third + GAP), y, third, ROW_H}
                                 : hexui::Rect{});
        back.setBounds({x + third + GAP, y, third, ROW_H});

        // Both modals sit centred on screen: they have no place in the column,
        // because while they are open the column is not listening.
        const float px = (context.width - PROMPT_W) / 2.0f;
        const float py = (context.height - PROMPT_H) / 2.0f;

        rename_name.setBounds({px + 24.0f, py + 74.0f, PROMPT_W - 48.0f, 44.0f});

        constexpr float PROMPT_ACTION_W = 150.0f;
        const float action_y = py + PROMPT_H - 58.0f;
        rename_cancel.setBounds({px + 24.0f, action_y, PROMPT_ACTION_W, 42.0f});
        rename_confirm.setBounds({px + PROMPT_W - 24.0f - PROMPT_ACTION_W, action_y,
                                  PROMPT_ACTION_W, 42.0f});

        delete_dialog.layout();
    }

    void LoadGameState::refresh() {
        saves = hexsave::listSaves();

        // Deleting the last save on a page would leave `first` past the end, so it
        // steps back to the last page that still holds something.
        while (first > 0 && first >= saves.size()) first -= SAVES_PER_PAGE;

        layout();
    }

    void LoadGameState::loadSave(const std::string& slot) {
        std::string error;

        const std::optional<hexsave::SaveData> data = hexsave::readFromFile(slot, error);
        if (!data) {
            message = context.loc.text(hexui::StringKey::MENU_LOAD_FAILED) + ": " + error;
            return;
        }

        const std::optional<std::vector<hex::Move>> moves = hexsave::materialise(*data, error);
        if (!moves) {
            message = context.loc.text(hexui::StringKey::MENU_SAVE_INVALID) + ": " + error;
            return;
        }

        // The save format derives the settings, since it knows how names are stored
        // in the file: this screen picks which match to reopen, not how to read it.
        // The clock keeps its current value, which the file does not record.
        const double seconds = context.settings.seconds_per_player;
        context.settings = hexsave::settingsFrom(*data);
        context.settings.seconds_per_player = seconds;

        context.pending_moves = *moves;

        // The black holes travel with the moves: without them a reloaded Arcade
        // match would resume on a full board, and the moves already played would
        // describe a position that never existed.
        context.pending_holes = data->holes;

        requestTransition(Transition::to(StateId::PLAYING));
    }

    // --- Deletion ------------------------------------------------------------

    void LoadGameState::openDeletePrompt(const std::string& slot) {
        target_slot = slot;
        message.clear();
        delete_dialog.open();
    }

    void LoadGameState::confirmDelete() {
        std::string error;

        message = hexsave::deleteSave(target_slot, error)
            ? std::string{}
            : context.loc.text(hexui::StringKey::SAVE_DELETE_FAILED) + ": " + error;

        target_slot.clear();
        refresh();
    }

    // --- Renaming ------------------------------------------------------------

    void LoadGameState::openRenamePrompt(const std::string& slot) {
        target_slot = slot;
        message.clear();
        rename_prompt = true;

        // Prefilled with the current name: renaming almost always means correcting
        // something already written, not starting over.
        rename_name.setText(slot);
        rename_name.setFocused(true);
    }

    void LoadGameState::closeRenamePrompt() {
        rename_prompt = false;
        rename_name.setFocused(false);
        target_slot.clear();
    }

    void LoadGameState::confirmRename() {
        // The same normalisation as saving: a name accepted here is one the game can
        // write back, and the two paths cannot diverge because the rule is written
        // once.
        const std::optional<std::string> slot = hexsave::normaliseSlotName(rename_name.text());
        if (!slot) {
            message = context.loc.text(hexui::StringKey::GAME_SAVE_BAD_NAME);
            return;
        }

        std::string error;
        if (!hexsave::renameSave(target_slot, *slot, error)) {
            message = context.loc.text(hexui::StringKey::SAVE_RENAME_FAILED) + ": " + error;
            return;
        }

        message.clear();
        closeRenamePrompt();
        refresh();
    }

    bool LoadGameState::handleRenameInput(const InputEvent& event) {
        if (!rename_prompt) return false;

        switch (event.type) {
            case InputType::RESIZED:
                return false;   // riguarda tutta la schermata, non solo il pannello

            case InputType::MOUSE_MOVED:
                rename_name.onMouseMove(event.x, event.y);
                rename_confirm.onMouseMove(event.x, event.y);
                rename_cancel.onMouseMove(event.x, event.y);
                return true;

            case InputType::MOUSE_LEFT:
                rename_name.onMouseLeave();
                rename_confirm.onMouseLeave();
                rename_cancel.onMouseLeave();
                return true;

            case InputType::MOUSE_PRESSED:
                if (event.button != MouseButton::LEFT) return true;

                if (rename_confirm.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    confirmRename();
                    return true;
                }
                if (rename_cancel.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    closeRenamePrompt();
                    return true;
                }

                // The field keeps focus even on an outside click: while the modal is
                // open there is nothing else that could take it.
                (void)rename_name.onMousePress(event.x, event.y);
                rename_name.setFocused(true);
                return true;

            case InputType::TEXT_ENTERED:
                (void)rename_name.onCharacter(event.text);
                return true;

            case InputType::KEY_PRESSED:
                if (event.key == Key::BACKSPACE)   (void)rename_name.onBackspace();
                else if (event.key == Key::ENTER)  confirmRename();
                else if (event.key == Key::ESCAPE) closeRenamePrompt();
                return true;

            default:
                return true;
        }
    }

    // --- Input and rendering -------------------------------------------------

    void LoadGameState::handleInput(const InputEvent& event) {
        // Both modals come first: while either is open the list below sees no event
        // at all.
        switch (delete_dialog.handleInput(event)) {
            case DialogOutcome::CONFIRMED:
                confirmDelete();
                return;
            case DialogOutcome::CANCELLED:
                target_slot.clear();
                return;
            case DialogOutcome::CONSUMED:
                return;
            case DialogOutcome::IGNORED:
                break;
        }

        if (handleRenameInput(event)) return;

        switch (event.type) {
            case InputType::RESIZED:
                layout();
                return;

            case InputType::MOUSE_MOVED:
                for (std::size_t i = 0; i < visibleCount(); ++i) {
                    entries[i].onMouseMove(event.x, event.y);
                    renames[i].onMouseMove(event.x, event.y);
                    deletes[i].onMouseMove(event.x, event.y);
                }
                if (hasPrev()) prev.onMouseMove(event.x, event.y);
                if (hasNext()) next.onMouseMove(event.x, event.y);
                back.onMouseMove(event.x, event.y);
                return;

            case InputType::MOUSE_LEFT:
                for (hexui::Button& b : entries) b.onMouseLeave();
                for (hexui::Button& b : renames) b.onMouseLeave();
                for (hexui::Button& b : deletes) b.onMouseLeave();
                prev.onMouseLeave();
                next.onMouseLeave();
                back.onMouseLeave();
                return;

            case InputType::MOUSE_PRESSED: {
                if (event.button != MouseButton::LEFT) return;

                for (std::size_t i = 0; i < visibleCount(); ++i) {
                    // The two commands are tried before the name: they sit inside
                    // the same row, and aiming at the bin is not asking to load.
                    if (deletes[i].onMousePress(event.x, event.y)) {
                        context.audio.play(hexassets::sounds::CLICK);
                        openDeletePrompt(saves[first + i]);
                        return;
                    }
                    if (renames[i].onMousePress(event.x, event.y)) {
                        context.audio.play(hexassets::sounds::CLICK);
                        openRenamePrompt(saves[first + i]);
                        return;
                    }
                    if (entries[i].onMousePress(event.x, event.y)) {
                        context.audio.play(hexassets::sounds::CLICK);
                        loadSave(saves[first + i]);
                        return;
                    }
                }

                if (hasPrev() && prev.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    first -= SAVES_PER_PAGE;
                    layout();
                    return;
                }
                if (hasNext() && next.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    first += SAVES_PER_PAGE;
                    layout();
                    return;
                }
                if (back.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    requestTransition(Transition::to(StateId::MAIN_MENU));
                }
                return;
            }

            case InputType::KEY_PRESSED:
                // Enter loads nothing: there is no obvious match it would mean. Only
                // Escape leaves, which is the expected cancellation.
                if (event.key == Key::ESCAPE) requestTransition(Transition::to(StateId::MAIN_MENU));
                return;

            default:
                return;
        }
    }

    void LoadGameState::update(const float dt) {
        // Only the field's caret blinks, and only while the field is visible.
        if (rename_prompt) rename_name.update(dt);
    }

    void LoadGameState::drawRenamePrompt(sf::RenderTarget& target) const {
        const hexgui::Theme& t = context.theme;
        const sf::Font* font = context.assets.font();

        // The veil says the list below has stopped listening, which is what makes
        // the panel read as modal rather than as one more box.
        sf::RectangleShape veil({context.width, context.height});
        veil.setFillColor(sf::Color(0, 0, 0, 150));
        target.draw(veil);

        const float px = (context.width - PROMPT_W) / 2.0f;
        const float py = (context.height - PROMPT_H) / 2.0f;

        hexgui::drawRoundedRect(target, {px, py, PROMPT_W, PROMPT_H}, 14.0f,
                                t.ui_surface, t.ui_border, 2.0f);

        hexgui::drawCenteredText(target, context.loc.text(hexui::StringKey::SAVE_RENAME_PROMPT),
                                 {px, py + 24.0f, PROMPT_W, 22 * 1.4f}, font, 22, t.hud_primary);

        hexgui::drawFlatTextInput(target, rename_name, font, t, hexgui::Icon::PENCIL);

        hexgui::drawFlatButton(target, rename_cancel, font, t, hexgui::ButtonStyle::GHOST);
        hexgui::drawFlatButton(target, rename_confirm, font, t, hexgui::ButtonStyle::PRIMARY,
                               hexgui::Icon::PENCIL);

        // Below the field: either why the name was rejected, or how to confirm.
        hexgui::drawCenteredText(target,
                                 message.empty()
                                     ? context.loc.text(hexui::StringKey::SAVE_MANAGE_HINT)
                                     : message,
                                 {px, py + 128.0f, PROMPT_W, 15 * 1.4f}, font, 15,
                                 message.empty() ? t.label : t.ui_accent);
    }

    void LoadGameState::draw(sf::RenderTarget& target) {
        context.backdrop.draw(target, context.width, context.height);

        const hexgui::Theme& t = context.theme;
        const sf::Font* font = context.assets.font();

        hexgui::drawGlowText(target, context.loc.text(hexui::StringKey::MENU_LOAD_TITLE),
                             {0.0f, context.height * 0.16f, context.width, 52 * 1.4f},
                             font, 52, t.hud_primary, t.ui_accent, 6.0f);

        if (saves.empty()) {
            hexgui::drawCenteredText(target, context.loc.text(hexui::StringKey::MENU_LOAD_EMPTY),
                                     {0.0f, context.height * 0.255f, context.width, 18 * 1.4f},
                                     font, 18, t.hud_secondary);
        }

        for (std::size_t i = 0; i < visibleCount(); ++i) {
            hexgui::drawFlatButton(target, entries[i], font, t, hexgui::ButtonStyle::SECONDARY,
                                   hexgui::Icon::FOLDER);

            // The two accessories are ghost buttons carrying an icon and no label,
            // so the eye scans the names and finds them only when looking.
            hexgui::drawFlatButton(target, renames[i], font, t, hexgui::ButtonStyle::GHOST,
                                   hexgui::Icon::PENCIL);
            hexgui::drawFlatButton(target, deletes[i], font, t, hexgui::ButtonStyle::GHOST,
                                   hexgui::Icon::TRASH);
        }

        if (hasPrev()) hexgui::drawFlatButton(target, prev, font, t, hexgui::ButtonStyle::GHOST);
        if (hasNext()) hexgui::drawFlatButton(target, next, font, t, hexgui::ButtonStyle::GHOST);
        hexgui::drawFlatButton(target, back, font, t, hexgui::ButtonStyle::GHOST,
                               hexgui::Icon::EXIT);

        // The screen-level message is hidden while the rename prompt is open: there
        // its place is inside the panel, next to what produced it, and the prompt
        // draws it itself.
        if (!message.empty() && !rename_prompt) {
            hexgui::drawCenteredText(target, message,
                                     {0.0f, context.height - 76.0f, context.width, 16 * 1.4f},
                                     font, 16, t.ui_accent);
        }

        hexgui::drawCenteredText(target, context.loc.text(hexui::StringKey::MENU_LOAD_HINT),
                                 {0.0f, context.height - 44.0f, context.width, 15 * 1.4f},
                                 font, 15, t.label);

        delete_dialog.draw(target);
        if (rename_prompt) drawRenamePrompt(target);
    }
}
