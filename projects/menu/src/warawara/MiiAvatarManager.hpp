#pragma once

#include <nxui/core/Types.hpp>
#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Texture.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#ifdef __SWITCH__
#include <switch.h>
#else
struct AccountUid {
    uint64_t uid[2];
    bool operator==(const AccountUid& o) const { return uid[0] == o.uid[0] && uid[1] == o.uid[1]; }
};
#endif

namespace warawara {

enum class MiiFavoriteColor : uint8_t {
    Red = 0,
    Orange = 1,
    Yellow = 2,
    Lime = 3,
    Green = 4,
    Blue = 5,
    Cyan = 6,
    Pink = 7,
    Purple = 8,
    Brown = 9,
    White = 10,
    Black = 11,
    Count = 12
};

/// Returns the authentic official hex/RGB color for a given Mii favorite color.
nxui::Color getFavoriteColorRgb(MiiFavoriteColor color);

/// Full data record for an extracted or generated Mii avatar.
struct MiiAvatarData {
    std::string nickname = "Mii";
    MiiFavoriteColor shirtColor = MiiFavoriteColor::Red;
    bool isUserAccount = false;
    AccountUid accountUid{};
    std::shared_ptr<nxui::Texture> headTexture;
    uint8_t gender = 0;   // 0 = Male, 1 = Female
    uint8_t height = 64;  // 0 - 128
    uint8_t build = 64;   // 0 - 128
    std::string source;   // "account", "database", "guest", "fallback"
};

class MiiAvatarManager {
public:
    MiiAvatarManager() = default;
    ~MiiAvatarManager() = default;

    /// Initializes and extracts all local console avatars, guest assets, and Mii DB.
    void initialize(nxui::GpuDevice& gpu, nxui::Renderer& ren, const std::string& assetBase = "romfs:");

    /// Clear all loaded textures and records.
    void clear();

    /// Whether initialization has completed and avatars are available.
    bool isInitialized() const { return m_initialized; }

    /// Returns list of all available extracted and guest avatars.
    const std::vector<MiiAvatarData>& avatars() const { return m_avatars; }

    /// Returns a random avatar from the pool.
    const MiiAvatarData& getRandomAvatar();

    /// Returns avatar matching user account UID, if available.
    const MiiAvatarData* getUserAvatar(const AccountUid& uid) const;

    /// Total count of available avatars.
    size_t count() const { return m_avatars.size(); }

private:
    void loadUserAccounts(nxui::GpuDevice& gpu, nxui::Renderer& ren);
    void loadSystemMiiDatabase(nxui::GpuDevice& gpu, nxui::Renderer& ren, const std::string& assetBase);
    void loadBundledGuests(nxui::GpuDevice& gpu, nxui::Renderer& ren, const std::string& assetBase);
    void ensureFallback(nxui::GpuDevice& gpu, nxui::Renderer& ren);

    std::shared_ptr<nxui::Texture> createProceduralAvatar(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                                                          const nxui::Color& skin, const nxui::Color& hair);

    bool m_initialized = false;
    std::vector<MiiAvatarData> m_avatars;
    std::vector<std::shared_ptr<nxui::Texture>> m_guestTextures;
    std::shared_ptr<nxui::Texture> m_fallbackTexture;
    size_t m_randomCursor = 0;
};

} // namespace warawara
