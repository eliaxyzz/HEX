/**
 * @file asset_manager.h
 * @brief Central manager for fonts, textures and sound samples.
 *
 * Every resource is loaded once and lent to whoever draws or plays it; callers
 * never take ownership.
 *
 * ### Generated placeholders
 *
 * When a texture is missing from assets/, the manager generates one at run time
 * with the intended appearance - a rounded rectangle with fill and border - and
 * caches it as though it had been loaded. The interface is therefore already
 * entirely sprite-based: the day a real PNG appears in the folder it is used in
 * the placeholder's stead without a line of code changing.
 *
 * ### Missing sounds
 *
 * A sound, unlike a texture, cannot be invented: a generated placeholder would be
 * noise, and noise is worse than silence. A missing audio file therefore produces
 * one note on clog - once only, since the negative result is cached too - and
 * soundBuffer() returns null. Callers tolerate the absence: the game is identical,
 * only mute.
 */

#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <filesystem>
#include <string>

#include "ui/resource_cache.h"
#include "ui/sfml_theme.h"

namespace hexassets {

    /** @brief Identifiers of the textures the interface uses. */
    namespace textures {
        inline constexpr const char* BUTTON_NORMAL   = "button_normal";
        inline constexpr const char* BUTTON_HOVER    = "button_hover";
        inline constexpr const char* BUTTON_PRESSED  = "button_pressed";
        inline constexpr const char* BUTTON_DISABLED = "button_disabled";
        inline constexpr const char* INPUT_NORMAL    = "input_normal";
        inline constexpr const char* INPUT_HOVER     = "input_hover";
        inline constexpr const char* INPUT_FOCUS     = "input_focus";
        inline constexpr const char* OPTION_NORMAL   = "option_normal";
        inline constexpr const char* OPTION_HOVER    = "option_hover";
        inline constexpr const char* OPTION_SELECTED = "option_selected";
    }

    /** @brief Identifiers of the sounds the game uses. */
    namespace sounds {
        /** @brief An interface command was activated. */
        inline constexpr const char* CLICK = "click";

        /** @brief A stone is placed on the board. */
        inline constexpr const char* PLACE = "place";

        /** @brief The winning chain appears: the match is decided. */
        inline constexpr const char* WIN = "win";
    }

    /**
     * @brief File name of the background track, inside the asset folder.
     * @note Not a resource identifier like the others: music bypasses the cache and
     * is streamed from disk. It lives here because this is where the game's file
     * names are looked for.
     */
    inline constexpr const char* BGM_FILE = "bgm.ogg";

    /** @brief Identifier of the interface font. */
    inline constexpr const char* UI_FONT = "ui";

    /**
     * @brief Side in pixels of the generated placeholders, and their border width.
     * @note The border is the part nine-slice drawing leaves undistorted.
     */
    inline constexpr int PLACEHOLDER_SIZE = 48;
    inline constexpr int PLACEHOLDER_BORDER = 14;

    /**
     * @brief Loads and retains fonts, textures and sound samples.
     * @note Constructed once per application and passed by reference.
     */
    class AssetManager {
    public:
        /**
         * @brief Builds the manager.
         * @param root Asset folder; missing files are not an error.
         * @param theme Palette used to generate the placeholders.
         */
        explicit AssetManager(std::filesystem::path root = "assets",
                              const hexgui::Theme& theme = hexgui::Theme::dark());

        /**
         * @brief Returns the interface font.
         *
         * Looks for <root>/<id>.ttf, falls back to the system fonts, and returns null
         * if none is found.
         *
         * @note Text drawing must tolerate a missing font: the application stays
         * usable without lettering.
         */
        [[nodiscard]] const sf::Font* font(const std::string& id = UI_FONT);

        /**
         * @brief Returns an interface texture.
         *
         * Looks for <root>/<id>.png and generates an equivalent one when absent.
         *
         * @return Always a valid texture.
         */
        [[nodiscard]] const sf::Texture& texture(const std::string& id);

        /**
         * @brief Returns a sound sample.
         *
         * Looks for <root>/<id>.ogg, then <root>/<id>.wav.
         *
         * @return Null if the file is missing or unreadable; the caller must tolerate
         * the absence by staying silent.
         * @warning The buffer belongs to the manager and lives as long as it does.
         * Anyone handing it to an sf::Sound must ensure that sound does not outlive
         * the manager.
         */
        [[nodiscard]] const sf::SoundBuffer* soundBuffer(const std::string& id);

        /** @brief Tests whether a texture came from a file rather than a placeholder. */
        [[nodiscard]] bool isFromFile(const std::string& id) const;

        /** @brief Returns how many resources have actually been materialised. */
        [[nodiscard]] std::size_t loadedFonts() const { return fonts.size(); }
        [[nodiscard]] std::size_t loadedTextures() const { return textures_cache.size(); }
        [[nodiscard]] std::size_t loadedSounds() const { return sounds_cache.size(); }

        /** @brief Returns the asset folder. */
        [[nodiscard]] const std::filesystem::path& assetRoot() const { return root; }

    private:
        /** @brief Folder the files are looked for in. */
        std::filesystem::path root;

        /** @brief Palette used for the placeholders. */
        hexgui::Theme theme;

        /** @brief Loaded fonts, by identifier. */
        ResourceCache<sf::Font> fonts;

        /** @brief Textures loaded or generated, by identifier. */
        ResourceCache<sf::Texture> textures_cache;

        /**
         * @brief Loaded sound samples, by identifier.
         * @note The cache remembers failures too, so a missing file is looked up once
         * rather than on every click.
         */
        ResourceCache<sf::SoundBuffer> sounds_cache;

        /** @brief Identifiers whose texture really came from a file. */
        std::vector<std::string> from_file;
    };
}

#endif //ASSET_MANAGER_H
