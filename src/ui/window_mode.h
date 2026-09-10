/**
 * @file window_mode.h
 * @brief Window creation and recreation.
 *
 * Switching between windowed and fullscreen under SFML means recreating the
 * window; it is not an attribute that can be toggled. The procedure lives here
 * rather than in the entry point because it is needed at two different moments, at
 * startup from the preferences and when the user changes their mind in the
 * settings, and because the details that must not be forgotten are always the
 * same: frame rate cap, antialiasing, view, and the size reported into the context.
 */

#ifndef WINDOW_MODE_H
#define WINDOW_MODE_H

#include <filesystem>

#include "states/sfml_app_state.h"

namespace hexapp {

    /** @brief Window size when not fullscreen. */
    inline constexpr unsigned WINDOW_W = 900;
    inline constexpr unsigned WINDOW_H = 800;

    /** @brief Requested antialiasing level; the driver may grant less. */
    inline constexpr unsigned ANTI_ALIASING = 8;

    /** @brief Frame rate the game is capped at. */
    inline constexpr unsigned FRAME_LIMIT = 60;

    /**
     * @brief Puts the window into the requested mode.
     *
     * Recreates the window, restores the frame cap and the view, and updates
     * context.width and context.height with the actual size: in fullscreen the
     * display decides it, not the game.
     *
     * @param context Context whose window is recreated.
     * @param fullscreen True for fullscreen.
     * @warning The caller must then re-lay out its widgets: the size changed
     * underneath it.
     */
    void applyWindowMode(AppContext& context, bool fullscreen);

    /** @brief Icon file name inside the asset folder. */
    inline constexpr const char* ICON_FILE = "icon.ico";

    /**
     * @brief Sets the window's title bar icon.
     *
     * Windows keeps two icons per window: the executable's, which comes from the
     * compiled resources (app.rc) and appears in the file explorer, and the
     * window's, which appears in the title bar and the taskbar. SFML registers its
     * window class without an icon, so the second stays empty even when the first
     * is in place, and the title bar shows the generic box.
     *
     * The icon is read from assets/icon.ico, falling back to the resource compiled
     * into the executable when the file is absent, as in a package shipped without
     * assets/. With neither available the window opens without an icon, which is no
     * reason to refuse to start.
     *
     * Does nothing outside Windows, where an application's icon is declared by the
     * desktop entry rather than by the process.
     *
     * @param window Window to apply the icon to.
     * @param asset_root Asset folder to look for icon.ico in.
     * @warning Must be called after every create(): a recreated window is new in
     * every respect and carries a different handle.
     */
    void applyWindowIcon(sf::RenderWindow& window, const std::filesystem::path& asset_root);
}

#endif //WINDOW_MODE_H
