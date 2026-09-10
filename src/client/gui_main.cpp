/**
 * @file gui_main.cpp
 * @brief Entry point of the graphical build: startup, main loop and shutdown.
 */

#include <SFML/Graphics.hpp>

#include <memory>
#include <optional>

#include "core/app_preferences.h"
#include "core/player_profile.h"
#include "states/app_state.h"
#include "ui/asset_manager.h"
#include "ui/audio_player.h"
#include "core/localization.h"
#include "states/load_game_state.h"
#include "states/main_menu_state.h"
#include "network/network_client.h"
#include "states/network_lobby_state.h"
#include "states/network_playing_state.h"
#include "states/playing_state.h"
#include "states/settings_state.h"
#include "states/tutorial_state.h"
#include "states/sfml_app_state.h"
#include "ui/sfml_input.h"
#include "ui/window_mode.h"

int main() {
    // --- Preferences ----------------------------------------------------------
    // Read before anything else: they decide how the window opens. A missing or
    // unreadable file yields the defaults, never a failed start.
    const hexapp::Preferences prefs = hexapp::loadPreferences();

    // Progression lives in its own file under saves/: it is player data, not
    // application configuration. Absent on a first run, which is correct.
    const hexapp::Profile profile = hexapp::loadProfile();

    // --- Window and shared context --------------------------------------------
    sf::RenderWindow window;

    // Assets live as long as the application: every font, texture and sound is
    // loaded once and lent to the screens.
    hexassets::AssetManager assets("assets", hexgui::Theme::dark());

    // Declared after the asset manager: the voices point into its buffers, so they
    // must be destroyed before it.
    hexgui::AudioPlayer audio(assets);
    audio.setMuted(prefs.muted);
    audio.setMusicVolume(static_cast<float>(prefs.music_volume));

    // Music is atmosphere: a missing track still starts the game with effects
    // only, exactly as a missing texture does.
    audio.playMusic(assets.assetRoot() / hexassets::BGM_FILE);

    // The connection lives as long as the application: the lobby and the online
    // match pass it through the context, and neither of them owns it.
    hexnet::NetworkClient net;

    // Language is a preference like any other: read at startup and in force until
    // the user changes it in the settings.
    hexui::LocalizationManager loc(prefs.language);

    // The theme is born with the accessibility choice already applied: the first
    // screen drawn must show the symbols, not wait for a trip through the settings
    // to notice them.
    hexgui::Theme theme = hexgui::Theme::dark();
    theme.colorblind = prefs.colorblind;

    // The board palette goes through the profile before being applied: one that is
    // chosen but no longer unlocked, from a hand-edited settings.ini or a reset
    // profile, falls back to classic instead of being honoured. There is a single
    // such check and it sits here, on the path every startup takes.
    hexgui::applyPalette(theme, hexapp::usablePalette(prefs.palette, profile.xp));

    hexapp::AppContext context{
        .window = window,
        .assets = assets,
        .audio = audio,
        .net = net,
        .loc = loc,
        .theme = theme,
        .width = static_cast<float>(hexapp::WINDOW_W),
        .height = static_cast<float>(hexapp::WINDOW_H),
        .prefs = prefs,
        .profile = profile
    };

    // Actually creates the window, in the mode the user last chose.
    hexapp::applyWindowMode(context, prefs.fullscreen);

    // --- State machine --------------------------------------------------------
    // The factory is the only place that knows the list of concrete screens.
    hexapp::StateMachine<hexapp::SfmlAppState> machine(
        [&context](const hexapp::StateId id) -> std::unique_ptr<hexapp::SfmlAppState> {
            switch (id) {
                case hexapp::StateId::PLAYING:
                    return std::make_unique<hexapp::PlayingState>(context);
                case hexapp::StateId::LOAD_GAME:
                    return std::make_unique<hexapp::LoadGameState>(context);
                case hexapp::StateId::SETTINGS:
                    return std::make_unique<hexapp::SettingsState>(context);
                case hexapp::StateId::NETWORK_LOBBY:
                    return std::make_unique<hexapp::NetworkLobbyState>(context);
                case hexapp::StateId::NETWORK_PLAYING:
                    return std::make_unique<hexapp::NetworkPlayingState>(context);
                case hexapp::StateId::TUTORIAL:
                    return std::make_unique<hexapp::TutorialState>(context);
                case hexapp::StateId::MAIN_MENU:
                    return std::make_unique<hexapp::MainMenuState>(context);
            }
            return nullptr;
        });

    machine.start(hexapp::StateId::MAIN_MENU);

    sf::Clock clock;

    while (window.isOpen() && machine.isRunning()) {
        // --- Events -----------------------------------------------------------
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                break;
            }

            // A resize concerns the window before it concerns the screen, so the
            // view is updated here, once.
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                context.width = static_cast<float>(resized->size.x);
                context.height = static_cast<float>(resized->size.y);
                window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {context.width, context.height})));
            }

            if (const std::optional<hexapp::InputEvent> input = hexapp::translateEvent(*event, window)) {
                machine.handleInput(*input);
            }
        }

        if (!window.isOpen()) break;

        // --- Update -----------------------------------------------------------
        const float dt = clock.restart().asSeconds();

        // The backdrop advances here rather than inside a screen: it belongs to the
        // application and must keep moving across screen changes.
        context.backdrop.update(dt);

        machine.update(dt);

        // --- Render -----------------------------------------------------------
        window.clear(context.theme.background);
        if (hexapp::SfmlAppState* state = machine.current()) state->draw(window);
        window.display();

        // --- Screen change ----------------------------------------------------
        // Outside any state method: the outgoing state is destroyed here, and
        // destroying it while one of its frames is on the stack is a use after free.
        machine.applyPendingTransition();
    }

    return 0;
}
