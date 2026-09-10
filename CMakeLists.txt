cmake_minimum_required(VERSION 3.22)
project(HEX)

set(CMAKE_CXX_EXTENSIONS OFF)

# src/ is the only directory on the include path, so every #include states which
# layer the header comes from: core/ ui/ states/ network/ server/ client/.
set(HEX_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Engine and rules: no SFML dependency.
set(HEX_CORE_SOURCES
        src/core/move.h
        src/core/board.h
        src/core/board.cpp
        src/core/game.h
        src/core/game.cpp
        src/core/mcts.h
        src/core/mcts.cpp
        src/core/player.h
        src/core/player.cpp
        src/core/game_ruler.h
        src/core/game_ruler.cpp
        src/core/game_observer.h
        src/core/game_controller.h
        src/core/game_controller.cpp
        src/ui/console_renderer.h
        src/ui/console_renderer.cpp
        src/core/test_players.h)

# GUI logic that is verifiable without a window.
set(HEX_GUI_LOGIC_SOURCES
        src/states/app_state.h
        src/states/app_state.cpp
        src/core/app_preferences.h
        src/core/app_preferences.cpp
        src/core/player_profile.h
        src/core/player_profile.cpp
        src/core/deferred_player.h
        src/core/deferred_player.cpp
        src/core/game_clock.h
        src/core/game_clock.cpp
        src/core/arcade.h
        src/core/arcade.cpp
        src/core/localization.h
        src/core/localization.cpp
        src/core/game_settings.h
        src/core/game_settings.cpp
        src/ui/ui_widgets.h
        src/ui/ui_widgets.cpp
        src/ui/resource_cache.h
        src/core/save_format.h
        src/core/save_format.cpp
        src/ui/animation.h
        src/core/winning_path.h
        src/core/winning_path.cpp
        src/core/hex_geometry.h
        src/core/hex_geometry.cpp
        src/ui/sfml_game_observer.h
        src/ui/sfml_game_observer.cpp
        src/ui/sfml_human_player.h
        src/ui/sfml_human_player.cpp)

# Rendering: requires SFML.
set(HEX_GUI_RENDER_SOURCES
        src/ui/sfml_theme.h
        src/ui/background_renderer.h
        src/ui/background_renderer.cpp
        src/ui/sfml_renderer.h
        src/ui/sfml_renderer.cpp
        src/states/sfml_app_state.h
        src/ui/ui_icons.h
        src/ui/ui_icons.cpp
        src/ui/sfml_widgets.h
        src/ui/sfml_widgets.cpp
        src/ui/sfml_input.h
        src/ui/sfml_input.cpp
        src/ui/asset_manager.h
        src/ui/asset_manager.cpp
        src/ui/audio_player.h
        src/ui/audio_player.cpp
        src/ui/window_mode.h
        src/ui/window_mode.cpp
        src/states/settings_state.h
        src/states/settings_state.cpp
        src/states/network_lobby_state.h
        src/states/network_lobby_state.cpp
        src/states/network_playing_state.h
        src/states/network_playing_state.cpp
        src/states/main_menu_state.h
        src/states/main_menu_state.cpp
        src/states/load_game_state.h
        src/states/load_game_state.cpp
        src/states/confirm_dialog.h
        src/states/confirm_dialog.cpp
        src/states/playing_state.h
        src/states/playing_state.cpp
        src/states/tutorial_state.h
        src/states/tutorial_state.cpp)

# Wire protocol shared by client and server.
set(HEX_NET_SOURCES
        src/network/network_protocol.h
        src/network/network_protocol.cpp
        src/network/network_client.h
        src/network/network_client.cpp
        src/network/network_match.h
        src/network/network_match.cpp)

# Referee for hosted matches: server only.
set(HEX_SERVER_SOURCES
        src/server/game_server.h
        src/server/game_server.cpp)

# --- Console executable -------------------------------------------------------
add_executable(HEX src/client/main.cpp ${HEX_CORE_SOURCES})
target_compile_features(HEX PRIVATE cxx_std_23)
target_include_directories(HEX PRIVATE ${HEX_SOURCE_ROOT})

# --- SFML 3 -------------------------------------------------------------------
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)   # static link: no DLLs to ship
set(SFML_BUILD_AUDIO ON CACHE BOOL "" FORCE)     # sound effects
set(SFML_BUILD_NETWORK ON CACHE BOOL "" FORCE)   # online multiplayer

FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.0.2
        GIT_SHALLOW ON
        SYSTEM)

FetchContent_MakeAvailable(SFML)

# --- Graphical executable -----------------------------------------------------
add_executable(HEX_GUI src/client/gui_main.cpp
        ${HEX_CORE_SOURCES}
        ${HEX_GUI_LOGIC_SOURCES}
        ${HEX_GUI_RENDER_SOURCES}
        ${HEX_NET_SOURCES})
target_compile_features(HEX_GUI PRIVATE cxx_std_23)
target_include_directories(HEX_GUI PRIVATE ${HEX_SOURCE_ROOT})
target_link_libraries(HEX_GUI PRIVATE SFML::Graphics SFML::Window SFML::System SFML::Audio SFML::Network)

# Icons are a Windows concept. A missing icon does not stop the build, but an
# app.rc naming one it cannot find would fail the resource compiler.
if(WIN32)
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/icon.ico")
        enable_language(RC)
        target_sources(HEX_GUI PRIVATE app.rc)

        # The path inside app.rc is relative to the source directory.
        set_source_files_properties(app.rc PROPERTIES
                INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}")
    else()
        message(STATUS "assets/icon.ico assente: HEX_GUI userà l'icona predefinita di Windows")
    endif()
endif()

# --- Headless server ----------------------------------------------------------
# No graphics: runs on a machine with neither a GPU nor a desktop.
add_executable(HEX_Server src/server/server_main.cpp
        ${HEX_CORE_SOURCES}
        ${HEX_GUI_LOGIC_SOURCES}
        ${HEX_NET_SOURCES}
        ${HEX_SERVER_SOURCES})
target_compile_features(HEX_Server PRIVATE cxx_std_23)
target_include_directories(HEX_Server PRIVATE ${HEX_SOURCE_ROOT})
target_link_libraries(HEX_Server PRIVATE SFML::Network SFML::System)

# --- Benchmark ----------------------------------------------------------------
# Measures rather than verifies, so it stays outside CTest: a drop in performance
# is not a logical failure.
add_executable(hex_bench bench/bench_mcts.cpp ${HEX_CORE_SOURCES})
target_compile_features(hex_bench PRIVATE cxx_std_23)
target_include_directories(hex_bench PRIVATE ${HEX_SOURCE_ROOT})

# --- Tests --------------------------------------------------------------------
# One CTest entry per group, so a failure names the area that broke.
enable_testing()

set(HEX_TEST_GROUPS
        rules
        console_view
        controller
        async_player
        engine
        history
        geometry
        hit_test
        gui_input
        gui_observer
        gui_loop
        gui_ai
        arbiter
        app_states
        ui_widgets
        preferences
        clock
        localization
        save_format
        winning_path
        arcade
        profile
        network)

add_executable(hex_tests
        tests/test_main.cpp
        tests/test_framework.h
        tests/test_doubles.h
        tests/test_rules.cpp
        tests/test_console_view.cpp
        tests/test_controller.cpp
        tests/test_async_player.cpp
        tests/test_engine.cpp
        tests/test_history.cpp
        tests/test_geometry.cpp
        tests/test_hit_test.cpp
        tests/test_gui_input.cpp
        tests/test_gui_observer.cpp
        tests/test_gui_loop.cpp
        tests/test_gui_ai.cpp
        tests/test_arbiter.cpp
        tests/test_app_states.cpp
        tests/test_ui_widgets.cpp
        tests/test_preferences.cpp
        tests/test_clock.cpp
        tests/test_localization.cpp
        tests/test_network.cpp
        tests/test_save_format.cpp
        tests/test_winning_path.cpp
        tests/test_arcade.cpp
        tests/test_profile.cpp
        ${HEX_CORE_SOURCES}
        ${HEX_GUI_LOGIC_SOURCES}
        ${HEX_NET_SOURCES})
target_compile_features(hex_tests PRIVATE cxx_std_23)
target_include_directories(hex_tests PRIVATE ${HEX_SOURCE_ROOT})

# The network module is needed, not a window: sf::Packet and sf::TcpSocket open
# nothing.
target_link_libraries(hex_tests PRIVATE SFML::Network SFML::System)

foreach(group IN LISTS HEX_TEST_GROUPS)
    add_test(NAME ${group} COMMAND hex_tests ${group})
endforeach()

# Explicit ceiling: turns a deadlock into a failure rather than a hung session.
set_tests_properties(${HEX_TEST_GROUPS} PROPERTIES TIMEOUT 300)

# --- Distribution package -----------------------------------------------------
#     cmake --build build --target package_release
#
# Executables, assets and compiler runtime in a single folder: copy it to a USB
# stick and it runs. SFML is static (BUILD_SHARED_LIBS OFF), audio included, so
# none of its DLLs need copying.
#
# A dedicated component because SFML, vendored through FetchContent, brings its own
# install rules for libraries, headers and CMake files, and those cannot be
# switched off from outside. The target below installs only this project's
# component.
set(HEX_PACKAGE_COMPONENT hex)

# The package is a folder to hand out, not a program to register with the system,
# so the default under "Program Files" is avoided.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/HEX_Release" CACHE PATH
        "Cartella del pacchetto pronto da distribuire" FORCE)
endif()

# The server ships with the game, so anyone can host a match.
if(WIN32)
    install(TARGETS HEX_GUI HEX_Server
            RUNTIME_DEPENDENCY_SET hex_runtime_deps
            RUNTIME DESTINATION . COMPONENT ${HEX_PACKAGE_COMPONENT})
else()
    install(TARGETS HEX_GUI HEX_Server
            RUNTIME DESTINATION . COMPONENT ${HEX_PACKAGE_COMPONENT})
endif()

# The assets have fallbacks, but the folder must exist: it is where a user can
# replace the files.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/"
        DESTINATION assets
        COMPONENT ${HEX_PACKAGE_COMPONENT})

if(WIN32)
    # Compiler runtime DLLs, resolved from the built executable. The Windows ones
    # are excluded: every machine already has them.
    get_filename_component(HEX_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    set(HEX_DEP_EXCLUDES "api-ms-.*" "ext-ms-.*")

    install(RUNTIME_DEPENDENCY_SET hex_runtime_deps
            PRE_EXCLUDE_REGEXES ${HEX_DEP_EXCLUDES}
            POST_EXCLUDE_REGEXES ".*[Ss]ystem32.*" ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss].*"
            DIRECTORIES "${HEX_COMPILER_DIR}"
            DESTINATION .
            COMPONENT ${HEX_PACKAGE_COMPONENT})
endif()

# A single command, so the component name need not be remembered.
add_custom_target(package_release
        COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}"
                --component ${HEX_PACKAGE_COMPONENT}
        DEPENDS HEX_GUI HEX_Server
        COMMENT "Pacchetto pronto da distribuire in ${CMAKE_INSTALL_PREFIX}")
