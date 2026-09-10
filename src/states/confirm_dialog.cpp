/**
 * @file confirm_dialog.cpp
 * @brief Confirmation dialog implementation.
 */

#include "states/confirm_dialog.h"

#include "ui/sfml_widgets.h"

namespace hexapp {

    namespace {
        /** @brief Panel width, in pixels. */
        constexpr float PANEL_W = 460.0f;

        /**
         * @brief Panel height: question, hint and two answers.
         * @note Sized for the tallest question the panel can receive, which is two
         * lines: the delete-save prompt does not fit on one line within PANEL_W in
         * every language.
         */
        constexpr float PANEL_H = 196.0f;

        /** @brief Width of each answer button. */
        constexpr float ACTION_W = 150.0f;

        /** @brief Height of each answer button. */
        constexpr float ACTION_H = 42.0f;

        /** @brief Panel inner padding. */
        constexpr float PADDING = 24.0f;

        /** @brief Question type size, in pixels. */
        constexpr unsigned PROMPT_SIZE = 22;

        /** @brief Baseline step between two question lines. */
        constexpr float PROMPT_LINE = PROMPT_SIZE * 1.4f;

        /** @brief Distance from the panel top edge to the first line. */
        constexpr float PROMPT_TOP = 26.0f;

        /** @brief Gap between the last question line and the hint. */
        constexpr float HINT_GAP = 12.0f;
    }

    ConfirmDialog::ConfirmDialog(AppContext& context, const hexui::StringKey prompt,
                                 const hexui::StringKey hint, const hexui::StringKey confirm_label,
                                 const hexgui::Icon confirm_icon)
        : context(context),
          prompt_key(prompt),
          hint_key(hint),
          confirm(context.loc.text(confirm_label), {}),
          cancel(context.loc.text(hexui::StringKey::GAME_CANCEL), {}),
          confirm_icon(confirm_icon) {
        layout();
    }

    void ConfirmDialog::layout() {
        const float x = (context.width - PANEL_W) / 2.0f;
        const float y = (context.height - PANEL_H) / 2.0f;
        const float action_y = y + PANEL_H - ACTION_H - 16.0f;

        // Cancel left, confirm right: the costly answer sits where the pointer does
        // not land by accident.
        cancel.setBounds({x + PADDING, action_y, ACTION_W, ACTION_H});
        confirm.setBounds({x + PANEL_W - PADDING - ACTION_W, action_y, ACTION_W, ACTION_H});
    }

    DialogOutcome ConfirmDialog::handleInput(const InputEvent& event) {
        if (!is_open) return DialogOutcome::IGNORED;

        switch (event.type) {
            case InputType::MOUSE_MOVED:
                confirm.onMouseMove(event.x, event.y);
                cancel.onMouseMove(event.x, event.y);
                return DialogOutcome::CONSUMED;

            case InputType::MOUSE_LEFT:
                confirm.onMouseLeave();
                cancel.onMouseLeave();
                return DialogOutcome::CONSUMED;

            case InputType::MOUSE_PRESSED:
                if (event.button != MouseButton::LEFT) return DialogOutcome::CONSUMED;

                if (confirm.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    close();
                    return DialogOutcome::CONFIRMED;
                }
                if (cancel.onMousePress(event.x, event.y)) {
                    context.audio.play(hexassets::sounds::CLICK);
                    close();
                    return DialogOutcome::CANCELLED;
                }
                return DialogOutcome::CONSUMED;

            case InputType::KEY_PRESSED:
                // The same two answers, from the keyboard.
                if (event.key == Key::ENTER) {
                    close();
                    return DialogOutcome::CONFIRMED;
                }
                if (event.key == Key::ESCAPE) {
                    close();
                    return DialogOutcome::CANCELLED;
                }
                return DialogOutcome::CONSUMED;

            case InputType::RESIZED:
                return DialogOutcome::IGNORED;   // concerns the whole screen

            default:
                return DialogOutcome::CONSUMED;
        }
    }

    void ConfirmDialog::draw(sf::RenderTarget& target) const {
        if (!is_open) return;

        const hexgui::Theme& t = context.theme;
        const sf::Font* font = context.assets.font();

        // The veil is not decoration: it signals that the screen below has stopped
        // listening, and is what makes the panel read as modal.
        sf::RectangleShape veil({context.width, context.height});
        veil.setFillColor(sf::Color(0, 0, 0, 150));
        target.draw(veil);

        const float x = (context.width - PANEL_W) / 2.0f;
        const float y = (context.height - PANEL_H) / 2.0f;

        hexgui::drawRoundedRect(target, {x, y, PANEL_W, PANEL_H}, 14.0f,
                                t.ui_surface, t.ui_border, 2.0f);

        // The hint goes below the last question line rather than at a fixed height:
        // wrapping is normal, since the two languages differ in length, and a fixed
        // height would overlap the two in whichever language runs longer.
        const std::size_t lines = hexgui::drawWrappedText(
            target, context.loc.text(prompt_key),
            {x + PADDING, y + PROMPT_TOP, PANEL_W - 2.0f * PADDING, 2.0f * PROMPT_LINE},
            font, PROMPT_SIZE, t.hud_primary);

        const float hint_y = y + PROMPT_TOP + static_cast<float>(lines) * PROMPT_LINE + HINT_GAP;
        hexgui::drawCenteredText(target, context.loc.text(hint_key),
                                 {x, hint_y, PANEL_W, 21.0f}, font, 15, t.label);

        hexgui::drawFlatButton(target, cancel, font, t, hexgui::ButtonStyle::GHOST);
        hexgui::drawFlatButton(target, confirm, font, t, hexgui::ButtonStyle::PRIMARY,
                               confirm_icon);
    }
}
