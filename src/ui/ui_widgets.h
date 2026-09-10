/**
 * @file ui_widgets.h
 * @brief Minimal interface widgets, free of SFML.
 *
 * This is the what: rectangles, hover state, hit testing, text editing. The how to
 * draw them lives in sfml_widgets.h. The same separation that lets geometry and
 * input be verified without opening a window.
 *
 * @note Deliberately few and plain: three widgets cover the entire menu, and every
 * extra feature - selection, arrow keys, clipboard - would multiply the cases to
 * handle without serving a nickname field.
 */

#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <cstddef>
#include <string>
#include <vector>

namespace hexui {

    /** @brief Rectangle in pixel space. */
    struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;

        /** @brief Tests whether a point falls inside the rectangle, borders included. */
        [[nodiscard]] bool contains(float px, float py) const;
    };

    /** @brief Current visual state of a widget. */
    enum class WidgetState { NORMAL, HOVERED, PRESSED, DISABLED };

    /**
     * @brief Rectangular button with a label.
     * @note Knows nothing of its own action: it reports having been activated and
     * its owner decides what that means.
     */
    class Button {
    public:
        /** @brief Builds the button with a label and bounds. */
        Button(std::string label, Rect bounds);

        /** @brief Bounds occupied. */
        [[nodiscard]] const Rect& bounds() const { return box; }
        void setBounds(const Rect& r) { box = r; }

        /** @brief Label displayed. */
        [[nodiscard]] const std::string& label() const { return text; }
        void setLabel(std::string l) { text = std::move(l); }

        /** @brief A disabled button does not react and draws dimmed. */
        [[nodiscard]] bool enabled() const { return is_enabled; }
        void setEnabled(bool e);

        /** @brief Returns the current visual state, for the drawing code. */
        [[nodiscard]] WidgetState state() const;

        /** @brief Updates the state as the pointer moves. */
        void onMouseMove(float x, float y);

        /** @brief Signals that the pointer left the usable area. */
        void onMouseLeave();

        /**
         * @brief Records a click.
         * @return true if the click landed on the button and it was enabled.
         */
        bool onMousePress(float x, float y);

    private:
        std::string text;
        Rect box;
        bool is_enabled = true;
        bool hovered = false;
        bool pressed = false;
    };

    /**
     * @brief Single-line text field.
     *
     * Accepts printable characters only and enforces a length limit before
     * insertion: an unbounded field ends up in a save file and in a status line that
     * cannot hold it.
     *
     * @note The text is kept as a sequence of code points and converted to UTF-8 on
     * demand, so a deletion removes a whole character rather than one byte.
     */
    class TextInput {
    public:
        /**
         * @brief Builds the field.
         * @param placeholder Grey text shown while the field is empty.
         * @param bounds Bounds occupied.
         * @param max_chars Maximum number of characters accepted.
         */
        TextInput(std::string placeholder, Rect bounds, std::size_t max_chars = 16);

        [[nodiscard]] const Rect& bounds() const { return box; }
        void setBounds(const Rect& r) { box = r; }

        /** @brief Returns the entered text, UTF-8 encoded. */
        [[nodiscard]] const std::string& text() const { return encoded; }

        /** @brief Returns the grey text shown while empty. */
        [[nodiscard]] const std::string& placeholder() const { return hint; }

        /** @brief Returns the number of characters entered. */
        [[nodiscard]] std::size_t length() const { return chars.size(); }

        /** @brief Returns the maximum number of characters accepted. */
        [[nodiscard]] std::size_t capacity() const { return max_chars; }

        /** @brief Tests whether the field holds focus and receives characters. */
        [[nodiscard]] bool focused() const { return has_focus; }
        void setFocused(bool f);

        /** @brief Returns the current visual state, for the drawing code. */
        [[nodiscard]] WidgetState state() const;

        void onMouseMove(float x, float y);
        void onMouseLeave();

        /**
         * @brief Records a click: the field takes focus if hit and loses it otherwise.
         * @return true if the click landed on the field.
         */
        bool onMousePress(float x, float y);

        /**
         * @brief Inserts a typed character.
         *
         * Control characters (below U+0020, and U+007F) are discarded, newline and
         * tab among them, as is anything exceeding the capacity.
         *
         * @return true if the character was inserted.
         * @note Deletion arrives through onBackspace(), not here.
         */
        bool onCharacter(char32_t codepoint);

        /** @brief Deletes the last character.
         * @return true if there was something to delete. */
        bool onBackspace();

        /** @brief Empties the field. */
        void clear();

        /**
         * @brief Replaces the contents with the given text.
         *
         * Used to prefill a field: renaming a save starts from the name it has, not
         * from an empty box to retype. The text goes through the same filters as
         * typing, control characters discarded and capacity respected, because
         * anything placed here must be something the user could have typed.
         *
         * @param utf8 The new contents, UTF-8 encoded.
         * @note Independent of focus: prefilling is done by the program rather than
         * by a typist, and requiring focus would let the order of two calls silently
         * decide whether the field fills or stays empty.
         */
        void setText(const std::string& utf8);

        /** @brief Advances the caret blink. */
        void update(float dt);

        /** @brief Tests whether the caret should be drawn right now. */
        [[nodiscard]] bool caretVisible() const { return has_focus && caret_on; }

    private:
        /** @brief Rebuilds the UTF-8 representation after a change. */
        void reencode();

        /**
         * @brief Appends a character, if printable and if there is room.
         * @note The rule for what the field accepts, saying nothing about from whom:
         * typing reaches it after the focus check, prefilling reaches it directly.
         * Writing it once is what guarantees the two paths accept the same things.
         */
        bool appendChar(char32_t codepoint);

        std::string hint;
        Rect box;
        std::size_t max_chars;

        /** @brief Characters entered, as code points. */
        std::u32string chars;

        /** @brief The same characters in UTF-8, ready for drawing. */
        std::string encoded;

        bool has_focus = false;
        bool hovered = false;

        /** @brief Accumulator and phase of the caret blink. */
        float caret_timer = 0.0f;
        bool caret_on = true;
    };

    /**
     * @brief Choice among mutually exclusive options laid out in a row.
     * @note The options divide the bounds into equal parts.
     */
    class OptionGroup {
    public:
        /**
         * @brief Builds the group.
         * @param options Labels; at least one.
         * @param bounds Overall bounds.
         * @param selected Index selected initially.
         */
        OptionGroup(std::vector<std::string> options, Rect bounds, int selected = 0);

        [[nodiscard]] const Rect& bounds() const { return box; }
        void setBounds(const Rect& r) { box = r; }

        /** @brief Returns the option labels. */
        [[nodiscard]] const std::vector<std::string>& options() const { return labels; }

        /** @brief Returns the index of the selected option. */
        [[nodiscard]] int selected() const { return chosen; }

        /** @brief Selects an option; out-of-range indices are ignored. */
        void setSelected(int index);

        /** @brief Returns the index of the option under the pointer, or -1. */
        [[nodiscard]] int hovered() const { return under_cursor; }

        /**
         * @brief Enables or disables a single option.
         *
         * A disabled option stays visible but unselectable: it ignores clicks, does
         * not light up under the pointer and cannot become the current choice. That
         * is what shows a reward which exists but is not yet unlocked; removing it
         * from the list would hide it, and an unseen reward is no incentive.
         *
         * @note Owned by the widget rather than by the drawing code because the same
         * rule must govern rendering and hit testing alike. Splitting them would
         * produce a greyed option that can still be pressed. Out-of-range indices are
         * ignored.
         */
        void setOptionEnabled(int index, bool enabled);

        /** @brief Tests whether an option can be selected. */
        [[nodiscard]] bool optionEnabled(int index) const;

        /** @brief Returns the bounds occupied by the given option. */
        [[nodiscard]] Rect optionBounds(int index) const;

        void onMouseMove(float x, float y);
        void onMouseLeave();

        /**
         * @brief Records a click on whichever option was hit.
         * @return true if the selection changed.
         */
        bool onMousePress(float x, float y);

    private:
        std::vector<std::string> labels;

        /**
         * @brief Which options may be selected; one entry per label.
         * @note All enabled at construction, so the common case costs nothing and a
         * group nobody locks behaves as if the feature were absent.
         */
        std::vector<bool> unlocked;

        Rect box;
        int chosen = 0;
        int under_cursor = -1;
    };
}

#endif //UI_WIDGETS_H
