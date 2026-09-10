/**
 * @file audio_player.cpp
 * @brief Audio player implementation.
 */

#include "ui/audio_player.h"

#include <algorithm>

namespace hexgui {

    AudioPlayer::AudioPlayer(hexassets::AssetManager& assets) : assets(assets) {}

    AudioPlayer::~AudioPlayer() {
        music.stop();
        for (const std::unique_ptr<sf::Sound>& voice : voices) voice->stop();
    }

    void AudioPlayer::play(const std::string& id) {
        if (is_muted) return;

        // Sample missing: AssetManager already reported it once, so simply stay
        // silent here.
        const sf::SoundBuffer* buffer = assets.soundBuffer(id);
        if (!buffer) return;

        // A stopped voice is ready for reuse.
        for (const std::unique_ptr<sf::Sound>& voice : voices) {
            if (voice->getStatus() == sf::Sound::Status::Stopped) {
                voice->setBuffer(*buffer);
                voice->play();
                return;
            }
        }

        if (voices.size() < MAX_VOICES) {
            voices.push_back(std::make_unique<sf::Sound>(*buffer));
            voices.back()->play();
            return;
        }

        // All busy: sacrifice them round-robin, oldest in reuse order first.
        // Truncating an effect beats dropping the new one, which is the response to
        // an action the player has just taken.
        sf::Sound& stolen = *voices[next_steal];
        next_steal = (next_steal + 1) % voices.size();

        stolen.stop();
        stolen.setBuffer(*buffer);
        stolen.play();
    }

    void AudioPlayer::setMuted(const bool muted) {
        is_muted = muted;

        // Mute is a master switch and covers the music too, which would otherwise
        // keep playing after the user asked for silence. The chosen volume is left
        // untouched and returns by itself on unmute.
        refreshMusic();

        if (!muted) return;

        // Muting while an effect is in flight must silence that one too, or its
        // tail arrives after the user's choice.
        for (const std::unique_ptr<sf::Sound>& voice : voices) voice->stop();
    }

    bool AudioPlayer::playMusic(const std::filesystem::path& file) {
        // An already open track must be closed first: openFromFile() replaces the
        // source underneath a stream that may still be running.
        music.stop();
        music_open = false;

        if (!std::filesystem::exists(file) || !music.openFromFile(file)) {
            // No music is not a fault: the game still plays its effects and behaves
            // exactly as before.
            return false;
        }

        music_open = true;
        music.setLooping(true);   // atmosfera, non colonna sonora: non deve finire

        refreshMusic();
        return true;
    }

    void AudioPlayer::stopMusic() {
        music.stop();
        music_open = false;
    }

    void AudioPlayer::setMusicVolume(const float percent) {
        music_volume = std::clamp(percent, 0.0f, 100.0f);
        refreshMusic();
    }

    bool AudioPlayer::musicPlaying() const {
        return music.getStatus() == sf::Music::Status::Playing;
    }

    void AudioPlayer::refreshMusic() {
        if (!music_open) return;

        const bool wanted = !is_muted && music_volume > 0.0f;

        if (!wanted) {
            // Paused rather than stopped: resuming must return the track to where it
            // was instead of restarting it on every volume adjustment.
            if (music.getStatus() == sf::Music::Status::Playing) music.pause();
            return;
        }

        music.setVolume(music_volume);
        if (music.getStatus() != sf::Music::Status::Playing) music.play();
    }
}
