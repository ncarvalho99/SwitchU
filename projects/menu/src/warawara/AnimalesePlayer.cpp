#include "AnimalesePlayer.hpp"
#include <cctype>
#include <algorithm>

namespace warawara {

AnimalesePlayer::AnimalesePlayer() {
    m_letterChunks.fill(nullptr);
    m_digitChunks.fill(nullptr);
}

AnimalesePlayer::~AnimalesePlayer() {
    clear();
}

bool AnimalesePlayer::initialize(const std::string& soundBase) {
    clear();

    // Ensure we have enough channels for speech alongside UI sfx
    Mix_AllocateChannels(16);

    int loadedCount = 0;

    // Load letter phonemes: a.wav - z.wav
    for (char c = 'a'; c <= 'z'; ++c) {
        std::string path = soundBase + "/" + c + ".wav";
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
        if (chunk) {
            Mix_VolumeChunk(chunk, static_cast<int>(m_volume * MIX_MAX_VOLUME));
            m_letterChunks[c - 'a'] = chunk;
            loadedCount++;
        }
    }

    // Load digit phonemes: 0.wav - 9.wav
    for (int i = 0; i <= 9; ++i) {
        std::string path = soundBase + "/" + std::to_string(i) + ".wav";
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
        if (chunk) {
            Mix_VolumeChunk(chunk, static_cast<int>(m_volume * MIX_MAX_VOLUME));
            m_digitChunks[i] = chunk;
            loadedCount++;
        }
    }

    m_ready = (loadedCount > 0);
    return m_ready;
}

void AnimalesePlayer::clear() {
    stop();

    for (auto*& chunk : m_letterChunks) {
        if (chunk) {
            Mix_FreeChunk(chunk);
            chunk = nullptr;
        }
    }

    for (auto*& chunk : m_digitChunks) {
        if (chunk) {
            Mix_FreeChunk(chunk);
            chunk = nullptr;
        }
    }

    m_ready = false;
}

void AnimalesePlayer::setVolume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
    int volInt = static_cast<int>(m_volume * MIX_MAX_VOLUME);

    for (auto* chunk : m_letterChunks) {
        if (chunk) Mix_VolumeChunk(chunk, volInt);
    }
    for (auto* chunk : m_digitChunks) {
        if (chunk) Mix_VolumeChunk(chunk, volInt);
    }
}

void AnimalesePlayer::speak(const std::string& text, uint8_t gender, float speedMultiplier) {
    if (!m_ready || text.empty()) return;

    stop();
    m_currentGender = gender;

    float baseDelay = (gender == 1) ? 0.060f : 0.075f;
    if (speedMultiplier > 0.1f) {
        baseDelay /= speedMultiplier;
    }

    // Build a natural cadence of phonemes from the text (capped to 6-12 syllables for charm)
    size_t count = 0;
    const size_t maxSyllables = 10;

    std::uniform_real_distribution<float> jitter(-0.012f, 0.012f);

    for (char raw : text) {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
        if (std::isalpha(c) || std::isdigit(c)) {
            float delay = std::max(0.040f, baseDelay + jitter(m_rng));
            m_phonemeQueue.push_back({c, delay});
            count++;
            if (count >= maxSyllables) {
                break;
            }
        }
    }

    m_queueIndex = 0;
    m_timer = 0.0f; // Play first phoneme immediately
}

void AnimalesePlayer::stop() {
    m_phonemeQueue.clear();
    m_queueIndex = 0;
    m_timer = 0.0f;
    // Halt speech audio channel (channel 14 reserved for Animalese)
    Mix_HaltChannel(14);
}

void AnimalesePlayer::playPhoneme(char c) {
    Mix_Chunk* chunk = nullptr;

    if (c >= 'a' && c <= 'z') {
        chunk = m_letterChunks[c - 'a'];
    } else if (c >= '0' && c <= '9') {
        chunk = m_digitChunks[c - '0'];
    }

    // Fallback: pick any available vowel for friendly tone if specific letter failed
    if (!chunk) {
        chunk = m_letterChunks['a' - 'a'];
    }

    if (chunk) {
        Mix_PlayChannel(14, chunk, 0);
    }
}

void AnimalesePlayer::update(float dt) {
    if (m_phonemeQueue.empty() || m_queueIndex >= m_phonemeQueue.size()) {
        return;
    }

    m_timer -= dt;
    if (m_timer <= 0.0f) {
        const auto& entry = m_phonemeQueue[m_queueIndex];
        playPhoneme(entry.character);

        m_timer = entry.delay;
        m_queueIndex++;

        if (m_queueIndex >= m_phonemeQueue.size()) {
            m_phonemeQueue.clear();
            m_queueIndex = 0;
        }
    }
}

} // namespace warawara
