/**
 * @file ui_widgets.cpp
 * @brief Widget implementation.
 */

#include "ui/ui_widgets.h"

#include <utility>

namespace hexui {

    namespace {
        /** @brief Duration of half a caret blink, in seconds. */
        constexpr float CARET_PERIOD = 0.53f;

        /** @brief Tests whether a code point is an acceptable printable character. */
        bool isPrintable(const char32_t cp) {
            if (cp < 0x20) return false;        // controlli, a capo, tabulazione
            if (cp == 0x7F) return false;       // cancella
            if (cp >= 0x80 && cp <= 0x9F) return false;   // controlli C1
            return cp <= 0x10FFFF;
        }

        /** @brief Appends a code point to a UTF-8 string. */
        void appendUtf8(std::string& out, const char32_t cp) {
            if (cp < 0x80) {
                out += static_cast<char>(cp);
            } else if (cp < 0x800) {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                out += static_cast<char>(0xF0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
    }

    // --- Rect ----------------------------------------------------------------

    bool Rect::contains(const float px, const float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }

    // --- Button --------------------------------------------------------------

    Button::Button(std::string label, const Rect bounds)
        : text(std::move(label)), box(bounds) {}

    void Button::setEnabled(const bool e) {
        is_enabled = e;
        if (!e) {
            hovered = false;
            pressed = false;
        }
    }

    WidgetState Button::state() const {
        if (!is_enabled) return WidgetState::DISABLED;
        if (pressed) return WidgetState::PRESSED;
        if (hovered) return WidgetState::HOVERED;
        return WidgetState::NORMAL;
    }

    void Button::onMouseMove(const float x, const float y) {
        if (!is_enabled) return;
        hovered = box.contains(x, y);
        if (!hovered) pressed = false;
    }

    void Button::onMouseLeave() {
        hovered = false;
        pressed = false;
    }

    bool Button::onMousePress(const float x, const float y) {
        if (!is_enabled || !box.contains(x, y)) return false;
        pressed = true;
        return true;
    }

    // --- TextInput -----------------------------------------------------------

    TextInput::TextInput(std::string placeholder, const Rect bounds, const std::size_t max_chars)
        : hint(std::move(placeholder)), box(bounds), max_chars(max_chars) {}

    void TextInput::setFocused(const bool f) {
        has_focus = f;
        caret_timer = 0.0f;
        caret_on = f;   // appena preso il fuoco il cursore si vede subito
    }

    WidgetState TextInput::state() const {
        if (has_focus) return WidgetState::PRESSED;
        if (hovered) return WidgetState::HOVERED;
        return WidgetState::NORMAL;
    }

    void TextInput::onMouseMove(const float x, const float y) {
        hovered = box.contains(x, y);
    }

    void TextInput::onMouseLeave() {
        hovered = false;
    }

    bool TextInput::onMousePress(const float x, const float y) {
        const bool hit = box.contains(x, y);
        setFocused(hit);
        return hit;
    }

    bool TextInput::appendChar(const char32_t codepoint) {
        if (!isPrintable(codepoint)) return false;
        if (chars.size() >= max_chars) return false;

        chars.push_back(codepoint);
        reencode();
        return true;
    }

    bool TextInput::onCharacter(const char32_t codepoint) {
        // Focus governs typing and nothing else: a character struck while the field
        // is not listening belongs to somebody else.
        if (!has_focus) return false;
        return appendChar(codepoint);
    }

    bool TextInput::onBackspace() {
        if (!has_focus || chars.empty()) return false;

        chars.pop_back();
        reencode();
        return true;
    }

    void TextInput::clear() {
        chars.clear();
        encoded.clear();
    }

    void TextInput::setText(const std::string& utf8) {
        clear();

        // Minimal UTF-8 decoding: read the length from the lead byte and accumulate
        // the continuation bytes. A stray byte does not fail the insertion, it ends
        // the character: this is a widget, not a validator, and appendChar() discards
        // whatever is unacceptable anyway.
        char32_t cp = 0;
        int remaining = 0;

        for (const unsigned char b : utf8) {
            if (remaining > 0) {
                if ((b & 0xC0) != 0x80) { remaining = 0; continue; }  // continuazione attesa
                cp = (cp << 6) | (b & 0x3F);
                if (--remaining == 0) (void)appendChar(cp);
                continue;
            }

            if (b < 0x80)             { (void)appendChar(b); }
            else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; remaining = 1; }
            else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; remaining = 2; }
            else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; remaining = 3; }
            // Any other lead byte is not valid UTF-8, so skip it.
        }
    }

    void TextInput::update(const float dt) {
        if (!has_focus) return;

        caret_timer += dt;
        while (caret_timer >= CARET_PERIOD) {
            caret_timer -= CARET_PERIOD;
            caret_on = !caret_on;
        }
    }

    void TextInput::reencode() {
        encoded.clear();
        for (const char32_t cp : chars) appendUtf8(encoded, cp);
    }

    // --- OptionGroup ---------------------------------------------------------

    OptionGroup::OptionGroup(std::vector<std::string> options, const Rect bounds, const int selected)
        : labels(std::move(options)), unlocked(labels.size(), true), box(bounds) {
        setSelected(selected);
    }

    bool OptionGroup::optionEnabled(const int index) const {
        if (index < 0 || index >= static_cast<int>(unlocked.size())) return false;
        return unlocked[static_cast<std::size_t>(index)];
    }

    void OptionGroup::setOptionEnabled(const int index, const bool enabled) {
        if (index < 0 || index >= static_cast<int>(unlocked.size())) return;
        unlocked[static_cast<std::size_t>(index)] = enabled;

        // Disabling the currently selected option would leave it selected and
        // impossible to leave, so the selection falls back to the first available
        // one, of which there is always at least one by construction.
        if (!enabled && chosen == index) {
            for (int i = 0; i < static_cast<int>(unlocked.size()); ++i) {
                if (optionEnabled(i)) { chosen = i; break; }
            }
        }
        if (under_cursor == index && !enabled) under_cursor = -1;
    }

    void OptionGroup::setSelected(const int index) {
        // Programmatic selection honours the lock too: a preference read from a file
        // must not activate what a click cannot.
        if (index >= 0 && index < static_cast<int>(labels.size()) && optionEnabled(index))
            chosen = index;
    }

    Rect OptionGroup::optionBounds(const int index) const {
        const int n = static_cast<int>(labels.size());
        if (n == 0 || index < 0 || index >= n) return {};

        const float w = box.w / static_cast<float>(n);
        return {box.x + w * static_cast<float>(index), box.y, w, box.h};
    }

    void OptionGroup::onMouseMove(const float x, const float y) {
        under_cursor = -1;
        for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
            // A locked option does not light up: the pointer over it promises
            // nothing, which is how the user finds out before clicking.
            if (optionEnabled(i) && optionBounds(i).contains(x, y)) {
                under_cursor = i;
                return;
            }
        }
    }

    void OptionGroup::onMouseLeave() {
        under_cursor = -1;
    }

    bool OptionGroup::onMousePress(const float x, const float y) {
        for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
            if (!optionEnabled(i)) continue;   // bloccata: il click le passa oltre
            if (optionBounds(i).contains(x, y)) {
                if (i == chosen) return false;
                chosen = i;
                return true;
            }
        }
        return false;
    }
}
