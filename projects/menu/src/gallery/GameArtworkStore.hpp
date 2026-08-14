#pragma once

#include "GameGalleryClient.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gallery {

enum class ArtworkKind { Cover, Background };

struct ArtworkSaveResult {
    bool ok = false;
    bool restored = false;
    std::uint64_t titleId = 0;
    ArtworkKind kind = ArtworkKind::Cover;
    std::string path;
    std::string error;
};

// Keeps selected SteamGridDB artwork independent of theme assets.  The next
// stages can load this stable local path without requesting the network again.
class GameArtworkStore {
public:
    static ArtworkSaveResult save(std::uint64_t titleId, ArtworkKind kind,
                                  const GameGalleryClient::Asset& asset,
                                  const std::vector<std::uint8_t>& bytes);
    // Removes only the selected custom slot; the native Switch icon/theme
    // background becomes visible again without touching the other slot.
    static ArtworkSaveResult restoreDefault(std::uint64_t titleId, ArtworkKind kind);

    // Resolves only the two files created by save(). This keeps the path used
    // by the home screen deterministic and never trusts metadata downloaded
    // from a remote catalog.
    static std::string pathFor(std::uint64_t titleId, ArtworkKind kind);
    static std::vector<std::uint8_t> loadCover(std::uint64_t titleId);
};

} // namespace gallery
