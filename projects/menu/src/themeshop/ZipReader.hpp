#pragma once

#include <cstdint>
#include <functional>
#include <string>

// Extracts a theme package into a directory.
//
// Deliberately small: it reads the archives the theme catalogue produces and
// nothing else. There is no zip64, no encryption, no multi-disk. A file that
// needs any of those is not one of ours, and guessing at it would be a way to
// hand the filesystem something unexpected.
//
// Every entry is checked before a byte is written. The archive arrives over the
// network from a catalogue anyone can contribute to, and it is unpacked into a
// directory the console then reads: absolute paths, traversal outside the
// destination, and unexpected file types are all refused.
//
// Reads from disk with seeks rather than from a buffer. A theme package is tens
// of megabytes, and holding one in memory -- after curl had already buffered
// and copied it -- peaked past 150 MB and killed the download with "Failed
// writing body". This way the cost is one entry at a time.
namespace themeshop {

struct ZipExtractResult {
    bool success = false;
    int filesWritten = 0;
    std::uint64_t bytesWritten = 0;
    std::string error;
};

// onProgress receives how many entries are done out of the total.
ZipExtractResult extractZipFile(const std::string& archivePath,
                                const std::string& destinationDir,
                                const std::function<void(int, int)>& onProgress = {});

} // namespace themeshop
