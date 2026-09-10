/**
 * @file audio_player.h
 * @brief Sound effect and music playback.
 *
 * The same separation as the rest of the view: AssetManager owns the samples, this
 * class owns only when and how they are played.
 *
 * ### Voices
 *
 * An sf::Sound plays one sample at a time. To avoid cutting off a sound still in
 * progress when another starts, the player keeps a small pool of voices and
 * assigns each request to a free one. The cap is deliberately low: this game has
 * three very short effects, and an unbounded pool would turn a burst of clicks
 * into an accumulation of audio resources.
 *
 * ### Silence is not an error
 *
 * If the file is missing, the sound device absent or the game muted, play() does
 * nothing and returns. No path in this class can interrupt a match.
 *
 * ### Music streams, samples do not
 *
 * An effect lasts under a second and sits in memory; a track lasts minutes and is
 * read from disk as it plays through sf::Music. Music therefore bypasses
 * AssetManager, which caches what it loads: caching a track would mean holding its
 * entire decoded waveform in RAM, precisely what streaming avoids.
 *
 * @note sf::Music owns the open file and its decoding thread for as long as it
 * lives. It is a member of this class rather than a pointer for that reason: it
 * closes when the player closes, with nobody having to remember.
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "ui/asset_manager.h"

namespace hexgui {

    /** @brief Maximum number of overlapping sounds. */
    inline constexpr std::size_t MAX_VOICES = 8;

    /** @brief Initial music volume, as a percentage. */
    inline constexpr float DEFAULT_MUSIC_VOLUME = 45.0f;

    /** @brief Plays the game's audio, or stays silent when it cannot. */
    class AudioPlayer {
    public:
        /**
         * @brief Builds the player.
         * @param assets Manager the samples come from.
         * @warning It must outlive the player, whose voices point into its buffers.
         */
        explicit AudioPlayer(hexassets::AssetManager& assets);

        /**
         * @brief Stops everything before the members are destroyed.
         * @note sf::Music and sf::Sound already stop what they are playing, but
         * member destruction order is not where one wants to discover a decoding
         * thread still alive. Stopping here makes the sequence explicit.
         */
        ~AudioPlayer();

        AudioPlayer(const AudioPlayer&) = delete;
        AudioPlayer& operator=(const AudioPlayer&) = delete;

        /**
         * @brief Plays the given effect.
         * @note Does nothing if the game is muted or the sample does not exist.
         */
        void play(const std::string& id);

        /** @brief Mutes or unmutes the game; muting stops whatever is playing. */
        void setMuted(bool muted);

        /** @brief Tests whether the game is muted. */
        [[nodiscard]] bool muted() const { return is_muted; }

        /** @brief Returns how many voices have been allocated so far. */
        [[nodiscard]] std::size_t voiceCount() const { return voices.size(); }

        // --- Background music -------------------------------------------------

        /**
         * @brief Opens a track and starts it looping.
         *
         * The file stays on disk and is read as it plays.
         *
         * @param file Path to the track.
         * @return true if the track was opened.
         * @note A missing or unreadable file is not an error: the music simply does
         * not start, as with the effects.
         */
        bool playMusic(const std::filesystem::path& file);

        /** @brief Stops the music and closes the file. */
        void stopMusic();

        /**
         * @brief Sets the music volume, 0 to 100.
         *
         * Independent of the effects: someone playing with the game in the background
         * can lower the music and keep the audio feedback of their own moves, which
         * is information rather than atmosphere.
         *
         * @note At 0 the track is paused rather than left running silently: a zero
         * volume means "not wanted", and there is no reason to keep reading the disk
         * in order not to be heard.
         */
        void setMusicVolume(float percent);

        /** @brief Returns the current music volume, 0 to 100. */
        [[nodiscard]] float musicVolume() const { return music_volume; }

        /** @brief Tests whether a track is playing right now. */
        [[nodiscard]] bool musicPlaying() const;

    private:
        /** @brief Sample manager. Non-owning. */
        hexassets::AssetManager& assets;

        /** @brief True when all playback must be suppressed. */
        bool is_muted = false;

        /**
         * @brief Allocated voices, created only when actually needed.
         * @note Held through unique_ptr because an sf::Sound cannot safely be moved
         * while playing: the references must stay put.
         */
        std::vector<std::unique_ptr<sf::Sound>> voices;

        /** @brief Next voice to recycle when all of them are busy. */
        std::size_t next_steal = 0;

        /**
         * @brief The playing track, streamed from disk.
         * @note Owns the open file and the decoding thread for its whole lifetime,
         * which is the player's.
         */
        sf::Music music;

        /** @brief True once a track has been opened successfully. */
        bool music_open = false;

        /** @brief Music volume requested by the user, 0 to 100. */
        float music_volume = DEFAULT_MUSIC_VOLUME;

        /**
         * @brief Aligns the track's state and volume with music_volume and the mute
         * flag.
         * @note A single place for a decision reached by three different callers.
         */
        void refreshMusic();
    };
}

#endif //AUDIO_PLAYER_H
