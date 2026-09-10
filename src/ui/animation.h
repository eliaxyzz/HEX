/**
 * @file animation.h
 * @brief Easing curves for the animations.
 *
 * @note SFML-free and stateless: pure functions, hence testable.
 */

#ifndef ANIMATION_H
#define ANIMATION_H

#include <algorithm>

namespace hexanim {

    /** @brief Duration of the stone placement animation, in seconds. */
    inline constexpr float PLACEMENT_DURATION = 0.17f;

    /**
     * @brief Returns the normalised progress of an animation.
     * @return 0 at the start, 1 at the end; saturates at 1 past the duration.
     */
    [[nodiscard]] constexpr float progress(const float elapsed, const float duration) {
        if (duration <= 0.0f) return 1.0f;
        return std::clamp(elapsed / duration, 0.0f, 1.0f);
    }

    /**
     * @brief Cubic ease-out: fast at first, settling at the end.
     * @note Preferred over an overshooting bounce: a stone expanding past its own
     * cell encroaches on its neighbours, and on a grid of touching cells that shows.
     */
    [[nodiscard]] constexpr float easeOutCubic(const float t) {
        const float x = std::clamp(t, 0.0f, 1.0f);
        const float inv = 1.0f - x;
        return 1.0f - inv * inv * inv;
    }

    /**
     * @brief Smoothstep interpolation: flat at 0 and at 1.
     * @note Rounds the corners off the triangular waves below. A pure triangle
     * reverses direction abruptly and the eye reads that as a jerk; this curve
     * reaches both ends with zero slope.
     */
    [[nodiscard]] constexpr float smoothstep(const float t) {
        const float x = std::clamp(t, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    /**
     * @brief Returns the fractional part of a cycle, always in [0, 1).
     * @note Written without std::fmod to stay constexpr and therefore checkable at
     * compile time: integer truncation suffices, and the negative branch handles a
     * clock running backwards.
     */
    [[nodiscard]] constexpr float phase(const float elapsed, const float period) {
        if (period <= 0.0f) return 0.0f;
        const float cycles = elapsed / period;
        const float frac = cycles - static_cast<float>(static_cast<long long>(cycles));
        return frac < 0.0f ? frac + 1.0f : frac;
    }

    // --- Ghost stone under the pointer ----------------------------------------

    /** @brief Period of one full ghost stone pulse, in seconds. */
    inline constexpr float GHOST_PULSE_PERIOD = 1.45f;

    /** @brief How far the ghost's opacity drops at the bottom of the pulse. */
    inline constexpr float GHOST_PULSE_DEPTH = 0.38f;

    /**
     * @brief Returns the ghost's opacity factor, between 1 - GHOST_PULSE_DEPTH and 1.
     * @note The pulse never reaches zero: a preview that vanished entirely would
     * read as "this cell is not playable", the opposite of what the ghost says.
     */
    [[nodiscard]] constexpr float ghostPulse(const float elapsed) {
        const float p = phase(elapsed, GHOST_PULSE_PERIOD);

        // Triangular wave 1 -> 0 -> 1, then rounded at both ends. It starts at full
        // rather than at the minimum: the timer resets on every new cell, and the
        // preview must be legible the instant it appears instead of fading in.
        const float triangle = p < 0.5f ? 1.0f - p * 2.0f : (p - 0.5f) * 2.0f;
        return 1.0f - GHOST_PULSE_DEPTH * (1.0f - smoothstep(triangle));
    }

    // --- Glow travelling along the winning chain ------------------------------

    /** @brief Seconds the glow takes to travel the chain. */
    inline constexpr float WIN_SWEEP_PERIOD = 1.9f;

    /**
     * @brief Trail length, in cells.
     * @note The glow starts outside the chain and leaves at the far end, so the
     * first and last links light up fully instead of appearing half lit.
     */
    inline constexpr float WIN_SWEEP_TAIL = 2.6f;

    /**
     * @brief Returns the position of the glow's head along the chain, in cells.
     *
     * Runs from -WIN_SWEEP_TAIL to cells - 1 + WIN_SWEEP_TAIL and repeats, so the
     * cycle is continuous with no frame in which the chain goes dark.
     *
     * @param elapsed Seconds since the match ended.
     * @param cells Number of cells in the winning chain.
     */
    [[nodiscard]] constexpr float sweepHead(const float elapsed, const int cells) {
        if (cells <= 0) return 0.0f;
        const float span = static_cast<float>(cells - 1) + 2.0f * WIN_SWEEP_TAIL;
        return phase(elapsed, WIN_SWEEP_PERIOD) * span - WIN_SWEEP_TAIL;
    }

    /**
     * @brief Returns how lit link `index` is with the head at `head`.
     * @return 1 directly under the head, 0 beyond the trail length.
     */
    [[nodiscard]] constexpr float sweepIntensity(const int index, const float head) {
        const float distance = static_cast<float>(index) - head;
        const float d = distance < 0.0f ? -distance : distance;
        if (d >= WIN_SWEEP_TAIL) return 0.0f;
        return smoothstep(1.0f - d / WIN_SWEEP_TAIL);
    }

    /**
     * @brief Base brightness of the chain, beneath the travelling glow.
     * @note The chain stays legible even while the head is elsewhere: the glow
     * underlines it rather than being the only thing that makes it visible.
     */
    inline constexpr float WIN_SWEEP_FLOOR = 0.22f;

    // --- Cell to touch, in the tutorial ---------------------------------------

    /** @brief Period of the pulse marking the cell to touch. */
    inline constexpr float HINT_PULSE_PERIOD = 1.1f;

    /**
     * @brief Returns the glow intensity on the marked cell, between 0.35 and 1.
     * @note Deeper than the ghost's pulse: that one accompanies a choice the player
     * has already made, this one has to draw the eye of somebody who does not yet
     * know where to look.
     */
    [[nodiscard]] constexpr float hintPulse(const float elapsed) {
        const float p = phase(elapsed, HINT_PULSE_PERIOD);
        const float triangle = p < 0.5f ? p * 2.0f : (1.0f - p) * 2.0f;
        return 0.35f + 0.65f * smoothstep(triangle);
    }

    // --- Animated backdrop ----------------------------------------------------

    /** @brief Scroll speed of the backdrop grid, in pixels per second. */
    inline constexpr float BACKDROP_SPEED = 7.0f;

    /** @brief Pulse period of a backdrop cell, in seconds. */
    inline constexpr float BACKDROP_PULSE_PERIOD = 6.5f;

    /** @brief Minimum and maximum opacity of a backdrop cell. */
    inline constexpr float BACKDROP_MIN_ALPHA = 0.10f;
    inline constexpr float BACKDROP_MAX_ALPHA = 0.42f;

    /**
     * @brief Returns a stable pseudo-random value derived from a cell's position.
     *
     * Depends on row and column only: the same cell must receive the same phase on
     * every frame, or the backdrop flickers instead of breathing. A real generator
     * would be wrong here for exactly that reason.
     *
     * Integer hash with Wang/Jenkins constants: it mixes the bits enough to hide
     * the diagonal banding a plain r + c would leave visible.
     *
     * @return A value in [0, 1).
     */
    [[nodiscard]] constexpr float cellSeed(const int row, const int col) {
        auto h = static_cast<unsigned int>(row * 73856093 ^ col * 19349663);
        h ^= h >> 16;
        h *= 0x7feb352du;
        h ^= h >> 15;
        h *= 0x846ca68bu;
        h ^= h >> 16;
        return static_cast<float>(h % 100000u) / 100000.0f;
    }

    /**
     * @brief Returns a backdrop cell's opacity, between BACKDROP_MIN and BACKDROP_MAX.
     *
     * Every cell starts at a different point of its cycle. That phase spread is what
     * conveys depth: cells pulsing in unison read as one plane blinking.
     *
     * @param seed The cell's own value in [0, 1), from cellSeed().
     */
    [[nodiscard]] constexpr float backdropAlpha(const float elapsed, const float seed) {
        const float p = phase(elapsed + seed * BACKDROP_PULSE_PERIOD, BACKDROP_PULSE_PERIOD);
        const float triangle = p < 0.5f ? p * 2.0f : (1.0f - p) * 2.0f;
        return BACKDROP_MIN_ALPHA
             + (BACKDROP_MAX_ALPHA - BACKDROP_MIN_ALPHA) * smoothstep(triangle);
    }
}

#endif //ANIMATION_H
