#pragma once

#include "PlazaSpeechBubble.hpp"
#include "MiiAvatarManager.hpp"
#include "activity/ActivityLogManager.hpp"

#include <string>
#include <vector>
#include <random>
#include <cstdint>

namespace warawara {

struct GameTipRecord {
    std::string game;
    std::vector<std::string> keywords;
    std::vector<std::string> tips;
};

/// Context-aware offline dialogue generator for WaraWara Plaza characters.
/// Combines bundled Nintendo Switch game tips, Horizon pdm / Activity Log playtime milestones,
/// system battery/time status, and classic authentic Miiverse posts.
class PlazaDialogueEngine {
public:
    PlazaDialogueEngine();
    ~PlazaDialogueEngine() = default;

    /// Loads dialogues from JSON database (e.g. "romfs:/data/plaza_dialogues.json").
    bool initialize(const std::string& assetBase = "romfs:");

    /// Whether the engine has loaded dialogues.
    bool isLoaded() const { return m_loaded; }

    /// Generates a contextual dialogue bubble for a specific Mii character.
    SpeechBubbleData getDialogueForMii(const MiiAvatarData& mii,
                                      const std::vector<std::pair<uint64_t, std::string>>& installedTitles,
                                      const switchu::activity::ActivityLogManager* activityLog = nullptr,
                                      int batteryPercent = 100,
                                      bool isCharging = false);

    /// Specific category queries
    SpeechBubbleData getGameTipDialogue(uint64_t titleId, const std::string& titleName, const std::string& author = "Mii");
    SpeechBubbleData getPlaytimeMilestoneDialogue(const switchu::activity::TitlePlayStats& stats, const std::string& author = "Mii");
    SpeechBubbleData getMiiverseDialogue(const std::string& author = "Mii");
    SpeechBubbleData getSystemStatusDialogue(int batteryPercent, bool isCharging, const std::string& author = "Mii");

private:
    void loadBuiltinFallbacks();
    bool parseJsonFile(const std::string& path);
    std::string findMatchingTip(const std::string& titleName);
    float randomFloat(float min, float max);
    int randomInt(int min, int max);

    bool m_loaded = false;
    std::vector<GameTipRecord> m_gameTips;
    std::vector<std::string>   m_genericTips;
    std::vector<std::string>   m_miiversePosts;

    mutable std::mt19937 m_rng{777};
};

} // namespace warawara
