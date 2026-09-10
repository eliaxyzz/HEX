/**
 * @file app_preferences.cpp
 * @brief Preferences parsing and serialisation.
 */

#include "core/app_preferences.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string_view>

namespace hexapp {

    namespace {
        /** @brief Keys recognised in the file. */
        constexpr const char* KEY_FULLSCREEN = "fullscreen";
        constexpr const char* KEY_MUTED = "muted";
        constexpr const char* KEY_COLORBLIND = "colorblind";
        constexpr const char* KEY_MUSIC_VOLUME = "music_volume";
        constexpr const char* KEY_WINS = "wins";
        constexpr const char* KEY_LOSSES = "losses";
        constexpr const char* KEY_LANGUAGE = "language";
        constexpr const char* KEY_PALETTE = "palette";

        /** @brief Counter ceiling: beyond it the file is corrupt, not prolific. */
        constexpr int MAX_COUNTER = 1'000'000;

        /** @brief Tokens accepted as true and as false. */
        constexpr std::array<const char*, 5> TRUE_WORDS{"1", "true", "on", "si", "yes"};
        constexpr std::array<const char*, 4> FALSE_WORDS{"0", "false", "off", "no"};

        /** @brief Strips leading and trailing whitespace. */
        std::string trim(const std::string& text) {
            const auto is_space = [](const unsigned char c) { return std::isspace(c) != 0; };

            auto begin = text.begin();
            while (begin != text.end() && is_space(static_cast<unsigned char>(*begin))) ++begin;

            auto end = text.end();
            while (end != begin && is_space(static_cast<unsigned char>(*(end - 1)))) --end;

            return {begin, end};
        }

        /** @brief Lowercases text for case-insensitive key and value matching. */
        std::string lowered(std::string text) {
            std::ranges::transform(text, text.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return text;
        }

        /**
         * @brief Parses a counter.
         * @param value Text to parse.
         * @param out Updated only when the text is a non-negative integer within
         * MAX_COUNTER; anything else leaves it untouched.
         */
        void assignCounter(const std::string& value, int& out) {
            if (value.empty() || value.size() > 7) return;
            for (const char c : value) {
                if (c < '0' || c > '9') return;
            }

            const long parsed = std::strtol(value.c_str(), nullptr, 10);
            if (parsed < 0 || parsed > MAX_COUNTER) return;

            out = static_cast<int>(parsed);
        }

        /**
         * @brief Parses a volume, 0 to 100.
         * @note Unlike a counter, an out-of-range value is clamped rather than
         * rejected: a file asking for 500 wants the music loud, and answering with
         * the default would surprise more than answering with 100.
         */
        void assignVolume(const std::string& value, int& out) {
            if (value.empty() || value.size() > 7) return;
            for (const char c : value) {
                if (c < '0' || c > '9') return;
            }

            const long parsed = std::strtol(value.c_str(), nullptr, 10);
            out = static_cast<int>(std::clamp(parsed, 0L, 100L));
        }

        /**
         * @brief Parses a language code.
         * @note An unknown code, from a newer build or a typo, leaves the preference
         * untouched: the default language is shown, the file is not rejected.
         */
        void assignLanguage(const std::string& value, hexui::Language& out) {
            if (const std::optional<hexui::Language> parsed =
                    hexui::LocalizationManager::languageFromCode(value)) {
                out = *parsed;
            }
        }

        /** @brief Returns the token a palette is written as. */
        std::string_view paletteToken(const BoardPalette p) {
            switch (p) {
                case BoardPalette::TOXIC:    return "toxic";
                case BoardPalette::PRESTIGE: return "prestige";
                default:                     return "classic";
            }
        }

        /**
         * @brief Parses a palette token.
         * @note An unknown token leaves the preference untouched, so a file from a
         * newer build renders in the classic scheme rather than blocking play.
         */
        void assignPalette(const std::string& value, BoardPalette& out) {
            if (value == "toxic")         out = BoardPalette::TOXIC;
            else if (value == "prestige") out = BoardPalette::PRESTIGE;
            else if (value == "classic")  out = BoardPalette::CLASSIC;
        }

        /**
         * @brief Parses a boolean value.
         * @param value Text to parse.
         * @param out Updated only when the text is recognised.
         */
        void assignBool(const std::string& value, bool& out) {
            if (std::ranges::find(TRUE_WORDS, value) != TRUE_WORDS.end()) {
                out = true;
                return;
            }
            if (std::ranges::find(FALSE_WORDS, value) != FALSE_WORDS.end()) out = false;
            // An unrecognised value leaves the preference as it was.
        }
    }

    std::string serializePreferences(const Preferences& prefs) {
        std::ostringstream out;
        out << "# HEX - preferences\n"
            << "# Generated by the game; safe to edit by hand.\n"
            << KEY_FULLSCREEN << '=' << (prefs.fullscreen ? 1 : 0) << '\n'
            << KEY_MUTED << '=' << (prefs.muted ? 1 : 0) << '\n'
            << KEY_COLORBLIND << '=' << (prefs.colorblind ? 1 : 0) << '\n'
            << KEY_MUSIC_VOLUME << '=' << prefs.music_volume << '\n'
            << KEY_WINS << '=' << prefs.wins << '\n'
            << KEY_LOSSES << '=' << prefs.losses << '\n'
            << KEY_LANGUAGE << '=' << hexui::LocalizationManager::codeOf(prefs.language) << '\n'
            << KEY_PALETTE << '=' << paletteToken(prefs.palette) << '\n';
        return out.str();
    }

    Preferences parsePreferences(const std::string& text) {
        Preferences prefs;   // defaults are the baseline; the file only amends them

        std::istringstream in(text);
        std::string line;

        while (std::getline(in, line)) {
            // An oversized line means a corrupt file, not a preference: skip it
            // before allocating anything based on its content.
            if (line.size() > MAX_PREFERENCE_LINE) continue;

            // Files written on Windows carry a trailing carriage return.
            if (!line.empty() && line.back() == '\r') line.pop_back();

            const std::string stripped = trim(line);
            if (stripped.empty() || stripped.front() == '#' || stripped.front() == ';') continue;

            // Section headers are ignored too: the key set is small enough that it
            // needs no grouping.
            if (stripped.front() == '[') continue;

            const std::size_t separator = stripped.find('=');
            if (separator == std::string::npos) continue;

            const std::string key = lowered(trim(stripped.substr(0, separator)));
            const std::string value = lowered(trim(stripped.substr(separator + 1)));
            if (key.empty() || value.empty()) continue;

            if (key == KEY_FULLSCREEN)   assignBool(value, prefs.fullscreen);
            else if (key == KEY_MUTED)   assignBool(value, prefs.muted);
            else if (key == KEY_COLORBLIND) assignBool(value, prefs.colorblind);
            else if (key == KEY_MUSIC_VOLUME) assignVolume(value, prefs.music_volume);
            else if (key == KEY_WINS)    assignCounter(value, prefs.wins);
            else if (key == KEY_LOSSES)  assignCounter(value, prefs.losses);
            else if (key == KEY_LANGUAGE) assignLanguage(value, prefs.language);
            else if (key == KEY_PALETTE)  assignPalette(value, prefs.palette);
            // Unknown keys are ignored so a file written by a newer build stays
            // readable by this one.
        }

        return prefs;
    }

    Preferences loadPreferences(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return {};   // first run: the defaults are exactly right

        std::ostringstream buffer;
        buffer << in.rdbuf();
        return parsePreferences(buffer.str());
    }

    bool savePreferences(const Preferences& prefs, const std::string& path) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) return false;

        out << serializePreferences(prefs);
        return static_cast<bool>(out);
    }
}
