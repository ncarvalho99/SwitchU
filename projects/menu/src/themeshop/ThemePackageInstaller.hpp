#pragma once

#include "ThemeCatalogClient.hpp"

#include <functional>
#include <string>

class ThemePackageInstaller {
public:
    enum class Mode {
        InstallOnly,
        InstallAndApply,
    };

    struct Result {
        bool success = false;
        bool installed = false;
        std::string themeId;
        std::string destinationPath;
        std::string message;
    };

    using ProgressCallback = std::function<void(std::string, float)>;

    static std::string destinationRootFor(const std::string& themeId, Mode mode);
    static Result run(const std::string& catalogUrl,
                      const ThemeCatalogClient::Entry& entry,
                      Mode mode,
                      ProgressCallback onProgress = {});
};