/**
 * @file app_state.h
 * @brief Application state machine.
 *
 * @note SFML-free: window events are translated into InputEvent before reaching
 * this layer, exactly as mouse clicks become logical coordinates before reaching
 * SfmlHumanPlayer. All transition logic is therefore verifiable without opening a
 * window.
 */

#ifndef APP_STATE_H
#define APP_STATE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace hexapp {

    /** @brief The application screens. */
    enum class StateId {
        MAIN_MENU,        ///< Opening menu.
        SETTINGS,         ///< Application preferences.
        PLAYING,          ///< Local match in progress.
        LOAD_GAME,        ///< Save slot selection.
        NETWORK_LOBBY,    ///< Connecting to an online match.
        NETWORK_PLAYING,  ///< Online match in progress.
        TUTORIAL          ///< Guided onboarding.
    };

    /**
     * @brief Screen change requested by a state.
     *
     * A state cannot destroy itself from inside one of its own methods, so it
     * declares the intent and the machine carries it out at the end of the frame.
     */
    struct Transition {
        enum class Kind {
            GO_TO,  ///< Switch to the given screen.
            QUIT    ///< Terminate the application.
        };

        Kind kind = Kind::GO_TO;
        StateId target = StateId::MAIN_MENU;

        /** @brief Builds a transition to the given screen. */
        [[nodiscard]] static Transition to(const StateId s) { return {Kind::GO_TO, s}; }

        /** @brief Builds an exit request. */
        [[nodiscard]] static Transition quit() { return {Kind::QUIT, StateId::MAIN_MENU}; }

        bool operator==(const Transition&) const = default;
    };

    // --- Backend-neutral input -----------------------------------------------

    /**
     * @brief Keys the application recognises.
     *
     * @note There is deliberately no undo key: a match cannot be taken back. See
     * PlayingState.
     */
    enum class Key : std::uint8_t {
        UNKNOWN, ENTER, ESCAPE, BACKSPACE,
        S,       ///< Pie rule.
        R        ///< Save.
    };

    /** @brief Mouse buttons the application recognises. */
    enum class MouseButton : std::uint8_t { LEFT, RIGHT, OTHER };

    /** @brief Input event category. */
    enum class InputType : std::uint8_t {
        KEY_PRESSED,
        MOUSE_MOVED,
        MOUSE_PRESSED,
        MOUSE_LEFT,     ///< The pointer left the window.
        TEXT_ENTERED,
        RESIZED
    };

    /**
     * @brief Input event, independent of the graphics library.
     * @note Fields irrelevant to the event type keep their defaults.
     */
    struct InputEvent {
        InputType type = InputType::KEY_PRESSED;

        /** @brief Key pressed, for KEY_PRESSED. */
        Key key = Key::UNKNOWN;

        /** @brief Button, for MOUSE_PRESSED. */
        MouseButton button = MouseButton::LEFT;

        /** @brief Pointer position, or the new size for RESIZED. */
        float x = 0.0f;
        float y = 0.0f;

        /** @brief Character typed, for TEXT_ENTERED. */
        char32_t text = 0;

        /** @brief Builds a key press event. */
        [[nodiscard]] static InputEvent keyPress(const Key k) {
            InputEvent e;
            e.type = InputType::KEY_PRESSED;
            e.key = k;
            return e;
        }

        /** @brief Builds a pointer movement event. */
        [[nodiscard]] static InputEvent mouseMove(const float x, const float y) {
            InputEvent e;
            e.type = InputType::MOUSE_MOVED;
            e.x = x;
            e.y = y;
            return e;
        }

        /** @brief Builds a click event. */
        [[nodiscard]] static InputEvent mousePress(const float x, const float y,
                                                   const MouseButton b = MouseButton::LEFT) {
            InputEvent e;
            e.type = InputType::MOUSE_PRESSED;
            e.x = x;
            e.y = y;
            e.button = b;
            return e;
        }

        /** @brief Builds a window resize event. */
        [[nodiscard]] static InputEvent resized(const float w, const float h) {
            InputEvent e;
            e.type = InputType::RESIZED;
            e.x = w;
            e.y = h;
            return e;
        }
    };

    // --- States ---------------------------------------------------------------

    /**
     * @brief One application screen.
     *
     * @note Rendering lives elsewhere: the concrete graphical states derive from
     * SfmlAppState, which adds draw(). That keeps this interface free of SFML and
     * lets tests substitute stub states.
     */
    class AppState {
    public:
        virtual ~AppState() = default;

        /** @brief Returns the screen identifier. */
        [[nodiscard]] virtual StateId id() const = 0;

        /** @brief Called once when the state becomes active. */
        virtual void onEnter() {}

        /** @brief Called once before the state is destroyed. */
        virtual void onExit() {}

        /** @brief Reacts to an input event. */
        virtual void handleInput(const InputEvent& event) = 0;

        /**
         * @brief Advances the state by one frame.
         * @param dt Seconds elapsed since the previous frame.
         */
        virtual void update(float dt) { (void)dt; }

        /** @brief Tests whether a transition has been requested. */
        [[nodiscard]] bool hasPendingTransition() const { return pending.has_value(); }

        /** @brief Takes the requested transition, clearing it. */
        [[nodiscard]] std::optional<Transition> takePendingTransition() {
            const std::optional<Transition> t = pending;
            pending.reset();
            return t;
        }

    protected:
        /**
         * @brief Requests a screen change.
         * @note The first request of a frame wins, so an event producing two does not
         * leave the outcome to evaluation order.
         */
        void requestTransition(const Transition t) {
            if (!pending) pending = t;
        }

    private:
        /** @brief Requested transition, not yet carried out. */
        std::optional<Transition> pending;
    };

    /**
     * @brief Drives the active state and performs its transitions.
     *
     * The `State` parameter lets the graphical build work with a type that can also
     * draw itself, without casts and without this file knowing about SFML; tests
     * instantiate it directly on AppState.
     */
    template <typename State>
    class StateMachine {
    public:
        /** @brief Builds the state matching an identifier. */
        using Factory = std::function<std::unique_ptr<State>(StateId)>;

        /** @brief Builds the machine around a state factory. */
        explicit StateMachine(Factory factory) : factory(std::move(factory)) {}

        /** @brief Installs the initial screen. */
        void start(const StateId first) {
            state = factory(first);
            if (state) state->onEnter();
        }

        /** @brief Forwards an event to the active state. */
        void handleInput(const InputEvent& event) {
            if (state) state->handleInput(event);
        }

        /** @brief Advances the active state by one frame. */
        void update(const float dt) {
            if (state) state->update(dt);
        }

        /**
         * @brief Performs the pending screen change, if any.
         *
         * @return true if a transition was performed.
         * @warning Call at the end of a frame, never from inside a state method: the
         * state is destroyed here, and destroying it while one of its frames is on
         * the stack is a use after free.
         */
        bool applyPendingTransition() {
            if (!state) return false;

            const std::optional<Transition> t = state->takePendingTransition();
            if (!t) return false;

            state->onExit();

            if (t->kind == Transition::Kind::QUIT) {
                state.reset();
                running = false;
            } else {
                state = factory(t->target);
                if (state) state->onEnter();
            }

            ++transitions;
            return true;
        }

        /** @brief Returns the active state, null after an exit transition. */
        [[nodiscard]] State* current() const { return state.get(); }

        /** @brief Returns the identifier of the active screen. */
        [[nodiscard]] std::optional<StateId> currentId() const {
            return state ? std::optional{state->id()} : std::nullopt;
        }

        /** @brief Tests whether the application is still running. */
        [[nodiscard]] bool isRunning() const { return running; }

        /** @brief Returns the number of transitions performed so far. */
        [[nodiscard]] int transitionCount() const { return transitions; }

    private:
        /** @brief State factory. */
        Factory factory;

        /** @brief Active state. */
        std::unique_ptr<State> state;

        /** @brief Cleared by an exit transition. */
        bool running = true;

        /** @brief Number of transitions performed. */
        int transitions = 0;
    };

    // --- Navigation rules ------------------------------------------------------
    //
    // Extracted from the concrete states because they are pure decisions: the
    // application's navigation stays covered by tests even though the screens using
    // it depend on SFML.

    /** @brief Returns the transition the menu makes for an event, if any. */
    [[nodiscard]] std::optional<Transition> menuTransitionFor(const InputEvent& event);

    /** @brief Returns the transition the match makes for an event, if any. */
    [[nodiscard]] std::optional<Transition> playingTransitionFor(const InputEvent& event);
}

#endif //APP_STATE_H
