#include "ThemePackageInstaller.hpp"

#include "ThemeHttp.hpp"

#include "core/DebugLog.hpp"

#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <nlohmann/json.hpp>
#include <switch.h>

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <list>
#include <stdexcept>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace {

struct GitHubRepoSource {
    std::string owner;
    std::string repo;
    std::string ref;
    std::string rawRoot;
};

struct RemoteFile {
    std::string relativePath;
    std::string downloadUrl;
};

std::string trimSlashes(std::string path) {
    while (!path.empty() && path.front() == '/')
        path.erase(path.begin());
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    return path;
}

std::string parentDirectory(const std::string& path) {
    std::string clean = trimSlashes(path);
    std::size_t slash = clean.find_last_of('/');
    if (slash == std::string::npos)
        return {};
    return clean.substr(0, slash);
}

std::string basename(const std::string& path) {
    std::string clean = trimSlashes(path);
    std::size_t slash = clean.find_last_of('/');
    if (slash == std::string::npos)
        return clean;
    return clean.substr(slash + 1);
}

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool pathExists(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
}

bool ensureDirectoryRecursive(const std::string& path) {
    if (path.empty())
        return false;

    std::string clean = path;
    while (!clean.empty() && clean.back() == '/')
        clean.pop_back();
    if (clean.empty())
        return false;

    std::size_t scheme = clean.find(":/");
    std::string current;
    std::size_t start = 0;
    if (scheme != std::string::npos) {
        current = clean.substr(0, scheme + 2);
        start = scheme + 2;
    }

    while (start < clean.size()) {
        std::size_t slash = clean.find('/', start);
        std::string part = clean.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!part.empty()) {
            if (!current.empty() && current.back() != '/')
                current.push_back('/');
            current += part;
            if (mkdir(current.c_str(), 0777) != 0 && errno != EEXIST)
                return false;
        }
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }

    return true;
}

bool removeDirectoryRecursive(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir)
        return std::remove(path.c_str()) == 0;

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;

        std::string child = path + "/" + name;
        struct stat st {};
        if (stat(child.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
            removeDirectoryRecursive(child);
        else
            std::remove(child.c_str());
    }

    closedir(dir);
    return rmdir(path.c_str()) == 0;
}

std::string joinUrl(const std::string& baseUrl, const std::string& relativePath) {
    if (relativePath.empty())
        return baseUrl;
    if (startsWith(relativePath, "https://") || startsWith(relativePath, "http://"))
        return relativePath;

    std::size_t slash = baseUrl.find_last_of('/');
    if (slash == std::string::npos)
        return relativePath;
    return baseUrl.substr(0, slash + 1) + trimSlashes(relativePath);
}

std::string joinPath(const std::string& base, const std::string& relative) {
    if (base.empty())
        return relative;
    if (relative.empty())
        return base;
    if (base.back() == '/')
        return base + relative;
    return base + "/" + relative;
}

bool parseRawGitHubUrl(const std::string& url, GitHubRepoSource& out) {
    static constexpr const char* kPrefix = "https://raw.githubusercontent.com/";
    if (!startsWith(url, kPrefix))
        return false;

    std::string rest = url.substr(std::strlen(kPrefix));
    std::size_t slash1 = rest.find('/');
    if (slash1 == std::string::npos)
        return false;
    std::size_t slash2 = rest.find('/', slash1 + 1);
    if (slash2 == std::string::npos)
        return false;
    std::size_t slash3 = rest.find('/', slash2 + 1);
    if (slash3 == std::string::npos)
        return false;

    out.owner = rest.substr(0, slash1);
    out.repo = rest.substr(slash1 + 1, slash2 - slash1 - 1);
    out.ref = rest.substr(slash2 + 1, slash3 - slash2 - 1);
    out.rawRoot = std::string(kPrefix) + out.owner + "/" + out.repo + "/" + out.ref;
    return !(out.owner.empty() || out.repo.empty() || out.ref.empty());
}

std::vector<RemoteFile> listThemeFilesFromTree(const GitHubRepoSource& repo,
                                               const std::string& themePath) {
    std::vector<RemoteFile> files;
    if (themePath.empty())
        return files;

    const std::string apiUrl = "https://api.github.com/repos/" + repo.owner + "/" + repo.repo
        + "/git/trees/" + repo.ref + "?recursive=1";
    const std::string body = themeshop::http::getText(apiUrl, {
        "Accept: application/vnd.github+json",
        "X-GitHub-Api-Version: 2022-11-28"
    });

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(body);
    } catch (const std::exception& ex) {
        throw std::runtime_error(std::string("Invalid GitHub tree JSON: ") + ex.what());
    }

    auto treeIt = root.find("tree");
    if (treeIt == root.end() || !treeIt->is_array())
        throw std::runtime_error("GitHub tree response is missing entries");

    const std::string prefix = trimSlashes(themePath) + "/";
    for (const auto& item : *treeIt) {
        if (!item.is_object())
            continue;

        auto typeIt = item.find("type");
        auto pathIt = item.find("path");
        if (typeIt == item.end() || pathIt == item.end() || !typeIt->is_string() || !pathIt->is_string())
            continue;
        if (typeIt->get<std::string>() != "blob")
            continue;

        std::string repoPath = trimSlashes(pathIt->get<std::string>());
        if (!startsWith(repoPath, prefix))
            continue;

        std::string relativePath = repoPath.substr(prefix.size());
        if (basename(relativePath) == ".gitkeep")
            continue;

        files.push_back({relativePath, repo.rawRoot + "/" + repoPath});
    }

    return files;
}

std::vector<RemoteFile> listThemeFiles(const std::string& catalogUrl,
                                       const std::string& themePath,
                                       const std::string& manifestPath) {
    std::vector<RemoteFile> files;

    GitHubRepoSource repo;
    if (parseRawGitHubUrl(catalogUrl, repo)) {
        try {
            files = listThemeFilesFromTree(repo, themePath);
        } catch (const std::exception& ex) {
            DebugLog::log("[themeshop] tree listing failed for %s: %s", themePath.c_str(), ex.what());
        }
    }

    if (files.empty() && !manifestPath.empty()) {
        std::string manifestName = basename(manifestPath);
        if (manifestName.empty())
            manifestName = "theme.json";
        files.push_back({manifestName, joinUrl(catalogUrl, manifestPath)});
    }

    std::sort(files.begin(), files.end(), [](const RemoteFile& lhs, const RemoteFile& rhs) {
        bool lhsManifest = lhs.relativePath == "theme.json";
        bool rhsManifest = rhs.relativePath == "theme.json";
        if (lhsManifest != rhsManifest)
            return lhsManifest;
        return lhs.relativePath < rhs.relativePath;
    });

    return files;
}

void writeFileBinary(const std::string& path, const std::string& data) {
    std::string parent = parentDirectory(path);
    if (!parent.empty() && !ensureDirectoryRecursive(parent))
        throw std::runtime_error("Failed to create directory: " + parent);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Failed to open file for writing: " + path);

    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!output)
        throw std::runtime_error("Failed to write file: " + path);
}

} // namespace

std::string ThemePackageInstaller::destinationRootFor(const std::string& themeId, Mode mode) {
    (void)mode;
    return "sdmc:/config/SwitchU/themes/" + themeId;
}

ThemePackageInstaller::Result ThemePackageInstaller::run(const std::string& catalogUrl,
                                                         const ThemeCatalogClient::Entry& entry,
                                                         Mode mode,
                                                         ProgressCallback onProgress) {
    Result result;
    result.themeId = entry.id;
    result.installed = true;
    result.destinationPath = destinationRootFor(entry.id, mode);

    const std::string themePath = trimSlashes(!entry.path.empty() ? entry.path : parentDirectory(entry.manifest));
    const std::string manifestPath = trimSlashes(!entry.manifest.empty() ? entry.manifest : joinPath(themePath, "theme.json"));
    if (manifestPath.empty())
        throw std::runtime_error("Catalog entry is missing a manifest path");

    if (onProgress) {
        onProgress(mode == Mode::InstallAndApply
                       ? "Preparing download + apply..."
                       : "Preparing install...",
                   0.f);
    }

    if (pathExists(result.destinationPath))
        removeDirectoryRecursive(result.destinationPath);
    if (!ensureDirectoryRecursive(result.destinationPath))
        throw std::runtime_error("Failed to prepare destination: " + result.destinationPath);

    std::vector<RemoteFile> files = listThemeFiles(catalogUrl, themePath, manifestPath);
    if (files.empty())
        throw std::runtime_error("Theme package contains no downloadable files");

    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto& file = files[i];
        float progress = files.size() > 1 ? (float)i / (float)files.size() : 0.f;
        if (onProgress) {
            onProgress("Downloading " + file.relativePath + " (" + std::to_string(i + 1)
                           + "/" + std::to_string(files.size()) + ")",
                       progress);
        }

        const std::string body = themeshop::http::getText(file.downloadUrl);
        writeFileBinary(joinPath(result.destinationPath, file.relativePath), body);
    }

    if (onProgress) {
        onProgress(mode == Mode::InstallAndApply
                       ? "Finalizing install before apply..."
                       : "Finalizing installation...",
                   1.f);
    }

    result.success = true;
    result.message = mode == Mode::InstallAndApply
        ? "Theme installed. Applying it now..."
        : "Theme installed.";
    return result;
}