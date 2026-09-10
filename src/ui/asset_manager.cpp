/**
 * @file asset_manager.cpp
 * @brief Asset manager implementation.
 */

#include "ui/asset_manager.h"

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <utility>

namespace hexassets {

    namespace {
        /** @brief System fonts tried when the assets provide none. */
        constexpr std::array<const char*, 4> SYSTEM_FONTS{
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/System/Library/Fonts/Helvetica.ttc"
        };

        /**
         * @brief Extensions tried for a sound sample, in order of preference.
         * @note OGG comes first because it costs a fraction of the WAV at comparable
         * quality, and these files ship in the distribution package.
         */
        constexpr std::array<const char*, 2> SOUND_EXTENSIONS{".ogg", ".wav"};

        /**
         * @brief Returns a pixel's coverage inside a rounded rectangle.
         * @note The value between 0 and 1 antialiases the edges instead of producing
         * a staircase: the texture is scaled up afterwards, where a staircase would
         * show.
         */
        float roundedCoverage(const int x, const int y, const int size, const float radius) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float s = static_cast<float>(size);

            // Distance from the inner rectangle, inset by the corner radius.
            const float dx = std::max({radius - px, 0.0f, px - (s - radius)});
            const float dy = std::max({radius - py, 0.0f, py - (s - radius)});
            const float distance = std::sqrt(dx * dx + dy * dy);

            return std::clamp(radius - distance + 0.5f, 0.0f, 1.0f);
        }

        /** @brief Blends two colours. */
        sf::Color mix(const sf::Color a, const sf::Color b, const float t) {
            const auto lerp = [t](const std::uint8_t x, const std::uint8_t y) {
                return static_cast<std::uint8_t>(static_cast<float>(x)
                                                 + (static_cast<float>(y) - static_cast<float>(x)) * t);
            };
            return {lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), lerp(a.a, b.a)};
        }

        /**
         * @brief Generates a bordered rounded rectangle, ready for nine-slice drawing.
         */
        std::unique_ptr<sf::Texture> makePlaceholder(const sf::Color fill, const sf::Color border) {
            constexpr int N = PLACEHOLDER_SIZE;
            constexpr float RADIUS = 10.0f;
            constexpr float EDGE = 1.6f;   // spessore del bordo in pixel

            sf::Image image(sf::Vector2u{N, N}, sf::Color::Transparent);

            for (int y = 0; y < N; ++y) {
                for (int x = 0; x < N; ++x) {
                    const float outside = roundedCoverage(x, y, N, RADIUS);
                    if (outside <= 0.0f) continue;

                    // A second, smaller rectangle separates border from fill.
                    const float inside = roundedCoverage(
                        static_cast<int>(static_cast<float>(x) - EDGE),
                        static_cast<int>(static_cast<float>(y) - EDGE),
                        static_cast<int>(static_cast<float>(N) - 2.0f * EDGE),
                        RADIUS - EDGE);

                    sf::Color c = mix(border, fill, std::clamp(inside, 0.0f, 1.0f));
                    c.a = static_cast<std::uint8_t>(static_cast<float>(c.a) * outside);

                    image.setPixel(sf::Vector2u{static_cast<unsigned>(x), static_cast<unsigned>(y)}, c);
                }
            }

            auto texture = std::make_unique<sf::Texture>();
            if (!texture->loadFromImage(image)) return nullptr;

            texture->setSmooth(true);
            return texture;
        }
    }

    AssetManager::AssetManager(std::filesystem::path root, const hexgui::Theme& theme)
        : root(std::move(root)),
          theme(theme),

          // --- Fonts: project assets first, then the system ---
          fonts([this](const std::string& id) -> std::unique_ptr<sf::Font> {
              auto font = std::make_unique<sf::Font>();

              const std::filesystem::path own = this->root / (id + ".ttf");
              std::error_code ec;
              if (std::filesystem::is_regular_file(own, ec) && font->openFromFile(own.string()))
                  return font;

              for (const char* path : SYSTEM_FONTS)
                  if (font->openFromFile(path)) return font;

              return nullptr;
          }),

          // --- Textures: the file when present, an equivalent placeholder otherwise ---
          textures_cache([this](const std::string& id) -> std::unique_ptr<sf::Texture> {
              const std::filesystem::path file = this->root / (id + ".png");

              std::error_code ec;
              if (std::filesystem::is_regular_file(file, ec)) {
                  auto loaded = std::make_unique<sf::Texture>();
                  if (loaded->loadFromFile(file.string())) {
                      loaded->setSmooth(true);
                      from_file.push_back(id);
                      return loaded;
                  }
                  // File present but unreadable: carry on with the placeholder, since
                  // an invisible interface is worse than a provisional one.
              }

              const hexgui::Theme& t = this->theme;

              if (id == textures::BUTTON_HOVER)    return makePlaceholder(t.ui_fill_hover, t.ui_border);
              if (id == textures::BUTTON_PRESSED)  return makePlaceholder(t.ui_fill_active, t.ui_accent);
              if (id == textures::BUTTON_DISABLED) return makePlaceholder(t.ui_fill_disabled, t.ui_fill_disabled);
              if (id == textures::INPUT_NORMAL)    return makePlaceholder(t.ui_fill, t.ui_border);
              if (id == textures::INPUT_HOVER)     return makePlaceholder(t.ui_fill_hover, t.ui_border);
              if (id == textures::INPUT_FOCUS)     return makePlaceholder(t.ui_fill_active, t.ui_accent);
              if (id == textures::OPTION_NORMAL)   return makePlaceholder(t.ui_fill, t.ui_border);
              if (id == textures::OPTION_HOVER)    return makePlaceholder(t.ui_fill_hover, t.ui_border);
              if (id == textures::OPTION_SELECTED) return makePlaceholder(t.ui_fill_active, t.ui_accent);

              return makePlaceholder(t.ui_fill, t.ui_border);   // BUTTON_NORMAL e ogni altro
          }),

          // --- Sounds: the file when present, declared silence otherwise ---
          sounds_cache([this](const std::string& id) -> std::unique_ptr<sf::SoundBuffer> {
              for (const char* extension : SOUND_EXTENSIONS) {
                  const std::filesystem::path file = this->root / (id + extension);

                  std::error_code ec;
                  if (!std::filesystem::is_regular_file(file, ec)) continue;

                  auto buffer = std::make_unique<sf::SoundBuffer>();
                  if (buffer->loadFromFile(file.string())) return buffer;

                  std::clog << "[audio] file illeggibile: " << file.string() << '\n';
              }

              // A sound cannot be generated: report the absence and stay quiet. The
              // cache remembers the failure, so this line appears once per identifier.
              std::clog << "[audio] suono assente: " << id << " (il gioco resta muto)\n";
              return nullptr;
          }) {}

    const sf::Font* AssetManager::font(const std::string& id) {
        return fonts.get(id);
    }

    const sf::Texture& AssetManager::texture(const std::string& id) {
        if (const sf::Texture* found = textures_cache.get(id)) return *found;

        // Last resort: one shared empty texture, so the reference stays valid even
        // if generating the placeholder failed too.
        static const sf::Texture empty;
        return empty;
    }

    const sf::SoundBuffer* AssetManager::soundBuffer(const std::string& id) {
        return sounds_cache.get(id);
    }

    bool AssetManager::isFromFile(const std::string& id) const {
        return std::ranges::find(from_file, id) != from_file.end();
    }
}
