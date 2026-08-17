#pragma once
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace switchu {

// Delete a file or a whole directory tree from the SD card.
//
// Written against POSIX rather than std::filesystem::remove_all, which is what
// theme deletion used and which does not work here: libstdc++ walks the tree
// with openat/fdopendir when it can, and fsdev provides neither for its device
// paths. remove_all then reports an error nobody was reading, the folder stayed
// on the card, and a player who removed a live wallpaper got the space back
// only in the list, never on the card. Reported after several themes had
// accumulated that way.
//
// A working copy of this already lived inside ThemePackageInstaller.cpp while
// the deletion path used the broken one, under the same name. It lives here now
// so there is one of it.
//
// failedPath, when given, receives the first path that could not be removed --
// the caller has the logger, this does not.
inline bool removeRecursive(const std::string& path, std::string* failedPath = nullptr) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        // Already gone is the outcome asked for.
        if (errno == ENOENT)
            return true;
        if (failedPath) *failedPath = path;
        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        if (std::remove(path.c_str()) == 0 || errno == ENOENT)
            return true;
        if (failedPath) *failedPath = path;
        return false;
    }

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        if (failedPath) *failedPath = path;
        return false;
    }

    bool ok = true;
    while (const dirent* entry = readdir(dir)) {
        const char* name = entry->d_name;
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0)
            continue;
        std::string child = path;
        if (!child.empty() && child.back() != '/')
            child.push_back('/');
        child += name;
        if (!removeRecursive(child, failedPath)) {
            ok = false;
            break;
        }
    }
    closedir(dir);

    if (!ok)
        return false;
    if (rmdir(path.c_str()) == 0 || errno == ENOENT)
        return true;

    if (failedPath) *failedPath = path;
    return false;
}

} // namespace switchu
