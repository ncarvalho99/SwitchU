#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Homebrew lives on the card as .nro files and never appears in
// nsListApplicationRecord, which is what the daemon builds the catalogue from.
// That is the whole reason installed homebrew has never shown up in the grid.
//
// An NRO carries its own name and icon in an asset section appended after the
// code, so nothing has to be guessed or configured: the same bytes hbmenu reads
// to draw its list are read here.
namespace homebrew {

struct Entry {
    std::string path;      ///< Absolute sdmc: path to the .nro
    std::string name;      ///< From the embedded NACP, or the filename
    std::string author;    ///< From the embedded NACP, may be empty
    std::uint64_t iconOffset = 0;  ///< Absolute file offset of the JPEG
    std::uint32_t iconSize = 0;    ///< 0 when the NRO carries no icon
};

/// Walks `root` for .nro files and reads each one's name and icon location.
/// Only the headers are read — a few hundred bytes per file — because the icon
/// bytes are several KB each and the grid needs them only for the page it is
/// showing. Recurses, since stores drop homebrew into subfolders.
std::vector<Entry> scan(const std::string& root, int maxDepth = 3);

/// Re-reads one NRO's headers, for the lazy icon path which has a path but no
/// Entry to hand.
bool readOne(const std::string& path, Entry& out);

/// Reads the icon JPEG for one entry, for the streamer to upload when the page
/// it belongs to becomes visible.
bool readIcon(const Entry& entry, std::vector<std::uint8_t>& out);

/// A stable id derived from the path. The grid, the saved layout and the icon
/// streamer are all keyed by title id, and an NRO has none — without one the
/// layout dropped every homebrew entry the moment it was added, because it
/// rebuilds the list from a map that only titles get into.
///
/// Bit 63 is set, which no real title id uses, so the two can never collide.
std::uint64_t syntheticTitleId(const std::string& path);

/// Remembers which path an id was made from, so the icon can be read later
/// rather than during the scan. Reading all of them up front cost 749ms on the
/// main thread with 42 homebrew installed.
void registerPath(std::uint64_t id, std::string path);
bool pathForId(std::uint64_t id, std::string& out);

}  // namespace homebrew
