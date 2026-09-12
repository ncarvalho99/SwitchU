#pragma once

#include <SDL2/SDL_mixer.h>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <random>

namespace warawara {

/// Synthesizes authentic Animalese / Plaza chatter using bundled phoneme audio clips.
class AnimalesePlayer {
public:
    AnimalesePlayer();
    ~AnimalesePlayer();

    /// Loads phonemes from soundBase directory (e.g. "romfs:/sounds/animalese").
    bool initialize(const std::string& soundBase = "romfs:/sounds/animalese");

    /// Frees loaded phoneme chunks.
    void clear();

    /// Whether phonemes are loaded and audio is ready.
    bool isReady() const { return m_ready; }

    /// Triggers speech chatter for a given sentence or phrase.
    /// @param text The sentence to speak.
    /// @param gender 0 = Male (standard pitch), 1 = Female (higher, faster cadence).
    /// @param speedMultiplier Cadence speed (default 1.0f).
    void speak(const std::string& text, uint8_t gender = 0, float speedMultiplier = 1.0f);

    /// Stop any ongoing speech playback.
    void stop();

    /// Whether speech chatter is currently actively playing.
    bool isSpeaking() const { return !m_phonemeQueue.empty(); }

    /// Volume level (0.0f to 1.0f).
    void setVolume(float volume);
    float volume() const { return m_volume; }

    /// Per-frame update for timed phoneme sequencing.
    void update(float dt);

private:
    void playPhoneme(char c);

    bool m_ready = false;
    float m_volume = 0.65f;

    // 26 letters (a-z) + 10 digits (0-9) = 36 phonemes
    std::array<Mix_Chunk*, 26> m_letterChunks{};
    std::array<Mix_Chunk*, 10> m_digitChunks{};

    struct PhonemeEntry {
        char character;
        float delay;
    };

    std::vector<PhonemeEntry> m_phonemeQueue;
    size_t m_queueIndex = 0;
    float  m_timer = 0.0f;
    float  m_currentDelay = 0.07f;
    uint8_t m_currentGender = 0;

    std::mt19937 m_rng{42};
};

} // namespace warawara
