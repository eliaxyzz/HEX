/**
 * @file window_mode.cpp
 * @brief Window creation implementation.
 */

#include "ui/window_mode.h"

#ifdef _WIN32
// Needed nowhere else, and its namespace is intrusive: included in this file
// only, after every other header.
#include <windows.h>
#endif

namespace hexapp {

#ifdef _WIN32

    namespace {
        /**
         * @brief Loads an icon at the requested size, from the file or the resources.
         *
         * @return The loaded icon, or null if there is no way to obtain one.
         * @note The two sizes are not incidental: an .ico holds several, and asking
         * for the right one stops Windows scaling the 256x256 down into the title
         * bar's 16 pixels, which is how a blurred icon is obtained instead of a crisp
         * one.
         */
        HICON loadIcon(const std::filesystem::path& file, const int size) {
            // The file first: it is the one a user can replace, and it is always
            // present in a package shipping assets/ beside the executable.
            if (HICON from_file = static_cast<HICON>(
                    LoadImageW(nullptr, file.wstring().c_str(), IMAGE_ICON,
                               size, size, LR_LOADFROMFILE))) {
                return from_file;
            }

            // Then the resource compiled into the executable: app.rc registers it
            // under identifier 1, and it survives a missing assets/ folder.
            return static_cast<HICON>(
                LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                           size, size, 0));
        }
    }

    void applyWindowIcon(sf::RenderWindow& window, const std::filesystem::path& asset_root) {
        const auto handle = static_cast<HWND>(window.getNativeHandle());
        if (handle == nullptr) return;

        const std::filesystem::path file = asset_root / ICON_FILE;

        // Windows wants two icons because it uses them in two places at different
        // resolutions: the small one in the title bar, the large one in Alt+Tab and
        // the taskbar. Supplying only one leaves the other empty.
        const int small_side = GetSystemMetrics(SM_CXSMICON);
        const int large_side = GetSystemMetrics(SM_CXICON);

        if (const HICON small_icon = loadIcon(file, small_side)) {
            SendMessageW(handle, WM_SETICON, ICON_SMALL,
                         reinterpret_cast<LPARAM>(small_icon));
        }
        if (const HICON large_icon = loadIcon(file, large_side)) {
            SendMessageW(handle, WM_SETICON, ICON_BIG,
                         reinterpret_cast<LPARAM>(large_icon));
        }
    }

#else

    void applyWindowIcon(sf::RenderWindow& window, const std::filesystem::path& asset_root) {
        // On Linux and macOS the process does not set the application icon: the
        // desktop entry or the bundle declares it. Nothing to do here.
        (void)window;
        (void)asset_root;
    }

#endif

    void applyWindowMode(AppContext& context, const bool fullscreen) {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = ANTI_ALIASING;

        sf::RenderWindow& window = context.window;

        if (fullscreen) {
            // The desktop mode is the only certainly supported one and avoids a
            // display resolution change: going fullscreen must not rearrange every
            // window on the system.
            window.create(sf::VideoMode::getDesktopMode(), "HEX",
                          sf::Style::None, sf::State::Fullscreen, settings);
        } else {
            // Title bar and close button, deliberately without Resize.
            //
            // Not an arbitrary restriction: the whole interface lays itself out from
            // the current width and height, and at extreme proportions - a squashed
            // window, or a very wide and short one - the rows overlap. Rather than
            // defending every screen against every possible aspect ratio, the
            // impossible ratios are removed: one size to draw well, and the layout
            // stays as designed. Fullscreen is the other supported size.
            //
            // Dropping Resize also drops the maximise button, which Windows enables
            // only for resizable windows.
            window.create(sf::VideoMode({WINDOW_W, WINDOW_H}), "HEX",
                          sf::Style::Titlebar | sf::Style::Close,
                          sf::State::Windowed, settings);
        }

        // A recreated window is new in every respect, so whatever was set on the
        // previous one must be set again. The icon included, which is why it lives
        // here and not in the entry point: it would otherwise be lost on the way
        // through a windowed-fullscreen-windowed round trip.
        window.setFramerateLimit(FRAME_LIMIT);
        applyWindowIcon(window, context.assets.assetRoot());

        const sf::Vector2u size = window.getSize();
        context.width = static_cast<float>(size.x);
        context.height = static_cast<float>(size.y);

        window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {context.width, context.height})));
    }
}
