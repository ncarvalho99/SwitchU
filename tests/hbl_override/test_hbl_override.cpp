// Host test for HblOverride, built against the real .cpp with only <switch.h>
// and DebugLog stubbed. It writes to the user's SD card, so being wrong here is
// expensive in a way a compile check does not catch.
//
// The paths in HblOverride are "sdmc:/..." -- no leading slash, so they are
// relative. Running from a scratch directory on Linux, where "sdmc:" is a legal
// directory name, exercises the real code unmodified.

#include "HblOverride.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_failures = 0;
static std::string g_case;

static void check(bool ok, const std::string& what) {
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n";
    if (!ok) ++g_failures;
}

static std::string slurp(const std::string& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}

static void put(const std::string& p, const std::string& body) {
    fs::create_directories(fs::path(p).parent_path());
    std::ofstream(p, std::ios::binary) << body;
}

static bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

static int countOf(const std::string& hay, const std::string& needle) {
    int n = 0;
    for (std::size_t i = hay.find(needle); i != std::string::npos;
         i = hay.find(needle, i + needle.size()))
        ++n;
    return n;
}

static const char* kIni = "sdmc:/atmosphere/config/override_config.ini";
static const char* kBak = "sdmc:/atmosphere/config/override_config.ini.switchu.bak";
static const char* kPrev = "sdmc:/config/SwitchU/hbl_prev_path";
static const char* kLoader = "sdmc:/switch/SwitchU/homebrew/hbl.nsp";

static void freshTree(bool withLoader = true) {
    fs::remove_all("sdmc:");
    if (withLoader) put(kLoader, "PFS0 pretend");
}

static void begin(const std::string& name) {
    g_case = name;
    std::cout << "\n== " << name << " ==\n";
}

int main() {
    fs::path scratch = fs::temp_directory_path() / "hbltest";
    fs::remove_all(scratch);
    fs::create_directories(scratch);
    fs::current_path(scratch);
    std::cout << "scratch: " << scratch << "\n";

    std::string err;

    // ---------------------------------------------------------------
    begin("enable on a console with no override file at all");
    freshTree();
    check(homebrew::enable(homebrew::AppletHost::ParentalControls, err),
          "enable succeeds (" + err + ")");
    {
        std::string ini = slurp(kIni);
        check(has(ini, "[hbl_config]"), "section created");
        check(has(ini, "program_id_0=0100000000001001"), "parental controls in slot 0");
        check(has(ini, "override_key_0=!R"), "override key holds R for the real applet");
        check(has(ini, "path=switch/SwitchU/homebrew/hbl.nsp"), "loader path points at ours");
        check(!fs::exists(kBak), "no backup written when there was no file");
        check(fs::exists(kPrev), "previous loader path recorded");
        check(slurp(kPrev).empty(), "previous path recorded as empty (was default)");
        check(!fs::exists("sdmc:/atmosphere/hbl.nsp"), "nothing written to atmosphere/hbl.nsp");
    }

    // ---------------------------------------------------------------
    begin("disable puts it back");
    check(homebrew::disable(err), "disable succeeds (" + err + ")");
    check(!fs::exists(kIni), "file removed, since nothing but our block was in it");
    check(!fs::exists(kPrev), "saved path cleaned up");

    // ---------------------------------------------------------------
    begin("someone already uses the Album with their own loader");
    freshTree();
    put(kIni,
        "[hbl_config]\n"
        "program_id_0=010000000000100D\n"
        "override_key_0=!R\n"
        "path=atmosphere/my_own_hbl.nsp\n"
        "\n"
        "[default_config]\n"
        "override_key=!L\n");
    check(homebrew::enable(homebrew::AppletHost::ParentalControls, err),
          "enable succeeds (" + err + ")");
    {
        std::string ini = slurp(kIni);
        check(has(ini, "program_id_0=010000000000100D"), "their Album override untouched");
        check(has(ini, "program_id_1=0100000000001001"), "ours took the next free slot");
        check(has(ini, "[default_config]"), "their other sections survived");
        check(has(ini, "override_key=!L"), "their default_config key survived");
        check(countOf(ini, "path=") == 2, "old path line still present, ours added");
        check(slurp(kPrev) == "atmosphere/my_own_hbl.nsp", "their loader path was saved");
        check(has(slurp(kBak), "my_own_hbl"), "backup holds the original file");
    }

    begin("disable restores their file");
    check(homebrew::disable(err), "disable succeeds (" + err + ")");
    {
        std::string ini = slurp(kIni);
        check(fs::exists(kIni), "their file kept, not deleted");
        check(has(ini, "program_id_0=010000000000100D"), "their Album override still there");
        check(has(ini, "[default_config]"), "their other sections still there");
        check(!has(ini, "0100000000001001"), "our program id gone");
        check(!has(ini, "switch/SwitchU/homebrew"), "our path line gone");
        check(!has(ini, "switchu:hbl"), "our markers gone");
    }

    // ---------------------------------------------------------------
    begin("enable twice does not stack up");
    freshTree();
    homebrew::enable(homebrew::AppletHost::ParentalControls, err);
    homebrew::enable(homebrew::AppletHost::Album, err);
    {
        std::string ini = slurp(kIni);
        check(countOf(ini, "switchu:hbl begin") == 1, "one block, not two");
        check(has(ini, "program_id_0=010000000000100D"), "second call switched host to Album");
        check(!has(ini, "0100000000001001"), "the parental controls entry was replaced");
        check(countOf(ini, "path=switch/SwitchU") == 1, "one path line, not two");
    }

    begin("inspect reports what is on the card");
    {
        homebrew::Status st = homebrew::inspect();
        check(st.overrideActive, "override seen as active");
        check(st.activeHost == homebrew::AppletHost::Album, "host read back as Album");
        check(st.loaderPresent, "loader seen on the card");
    }

    // ---------------------------------------------------------------
    // The log said "previous loader path: switch/SwitchU/..." on the second
    // enable, meaning the record of what the console used had been replaced by
    // our own. Nothing read it back yet, so nothing broke -- which is exactly
    // how it would have survived to the point where something did.
    begin("switching host must not lose the loader path they started with");
    freshTree();
    put(kIni,
        "[hbl_config]\n"
        "path=atmosphere/my_own_hbl.nsp\n");
    homebrew::enable(homebrew::AppletHost::ParentalControls, err);
    homebrew::enable(homebrew::AppletHost::Album, err);
    check(slurp(kPrev) == "atmosphere/my_own_hbl.nsp",
          "still records their path, not ours (got: '" + slurp(kPrev) + "')");
    homebrew::disable(err);
    {
        std::string ini = slurp(kIni);
        check(has(ini, "path=atmosphere/my_own_hbl.nsp"), "their loader path is back");
        check(!has(ini, "switch/SwitchU/homebrew"), "ours is gone");
        check(countOf(ini, "path=") == 1, "exactly one path line remains");
    }

    // ---------------------------------------------------------------
    begin("all eight slots taken");
    freshTree();
    {
        std::string full = "[hbl_config]\n";
        for (int i = 0; i < 8; ++i)
            full += "program_id_" + std::to_string(i) + "=010000000000100" +
                    std::string(1, (char)('0' + i)) + "\n";
        put(kIni, full);
        err.clear();
        check(!homebrew::enable(homebrew::AppletHost::ParentalControls, err),
              "enable refuses rather than clobbering a slot");
        check(!err.empty(), "and says why: " + err);
    }

    // ---------------------------------------------------------------
    begin("the loader is missing from the card");
    freshTree(false);
    err.clear();
    check(!homebrew::enable(homebrew::AppletHost::ParentalControls, err),
          "enable refuses without a loader to point at");
    check(!fs::exists(kIni), "and wrote nothing");

    // ---------------------------------------------------------------
    begin("disable on a console that was never enabled");
    freshTree();
    err.clear();
    check(homebrew::disable(err), "disable is harmless (" + err + ")");

    begin("disable leaves an unrelated file alone");
    put(kIni, "[hbl_config]\nprogram_id_0=010000000000100D\n");
    check(homebrew::disable(err), "disable succeeds");
    check(has(slurp(kIni), "010000000000100D"), "their override untouched");

    std::cout << "\n" << (g_failures == 0 ? "todos passaram" :
                          std::to_string(g_failures) + " FALHARAM") << "\n";
    return g_failures == 0 ? 0 : 1;
}
