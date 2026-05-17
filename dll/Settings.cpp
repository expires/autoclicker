#include "Settings.h"
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <string>

#pragma comment(lib, "shell32.lib")

// Resolves %APPDATA%\manuclicker\config.cfg, creating the directory if needed.
// Returns "" on any failure — Save/Load then become silent no-ops, which is
// the right behavior since this is best-effort persistence, not load-bearing.
static std::string ConfigPath()
{
    char path[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
        return {};
    std::string dir = std::string(path) + "\\manuclicker";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir + "\\config.cfg";
}

void Settings::Save()
{
    const std::string path = ConfigPath();
    if (path.empty()) {
        printf("[AC] Settings::Save: ConfigPath() returned empty — APPDATA unresolvable\n");
        return;
    }
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "w") != 0 || !f) {
        printf("[AC] Settings::Save: fopen_s failed for %s\n", path.c_str());
        return;
    }

    fprintf(f, "acEnabled=%d\n",    acEnabled    ? 1 : 0);
    fprintf(f, "breakBlocks=%d\n",  breakBlocks  ? 1 : 0);
    fprintf(f, "cps=%d\n",          cps);
    fprintf(f, "espEnabled=%d\n",   espEnabled   ? 1 : 0);
    fprintf(f, "drawBox=%d\n",      drawBox      ? 1 : 0);
    fprintf(f, "drawName=%d\n",     drawName     ? 1 : 0);
    fprintf(f, "drawDistance=%d\n", drawDistance ? 1 : 0);
    fprintf(f, "menuKey=%d\n",      menuKey);
    fprintf(f, "acKey=%d\n",        acKey);
    fprintf(f, "espKey=%d\n",       espKey);
    fprintf(f, "version=%d\n",      version);

    fflush(f);
    fclose(f);
    printf("[AC] Settings::Save wrote cps=%d to %s\n", cps, path.c_str());
}

void Settings::Load()
{
    const std::string path = ConfigPath();
    if (path.empty()) {
        printf("[AC] Settings::Load: ConfigPath() returned empty\n");
        return;
    }
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "r") != 0 || !f) {
        printf("[AC] Settings::Load: no config at %s (fresh install or load failure)\n", path.c_str());
        return;
    }
    printf("[AC] Settings::Load reading %s\n", path.c_str());

    // Assume legacy / pre-versioned until proven otherwise; a missing
    // `version=` line then triggers the migration block below.
    version = 0;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64] = {};
        int  val     = 0;
        if (sscanf_s(line, "%63[^=]=%d", key, (unsigned)sizeof(key), &val) != 2)
            continue;

        const std::string k = key;
        if      (k == "acEnabled")    acEnabled    = (val != 0);
        else if (k == "breakBlocks")  breakBlocks  = (val != 0);
        else if (k == "cps")          cps          = val;
        else if (k == "espEnabled")   espEnabled   = (val != 0);
        else if (k == "drawBox")      drawBox      = (val != 0);
        else if (k == "drawName")     drawName     = (val != 0);
        else if (k == "drawDistance") drawDistance = (val != 0);
        else if (k == "menuKey")      menuKey      = val;
        else if (k == "acKey")        acKey        = val;
        else if (k == "espKey")       espKey       = val;
        else if (k == "version")      version      = val;
    }

    fclose(f);
    printf("[AC] Settings::Load done — cps=%d  espKey=%d  acKey=%d  menuKey=%d  version=%d\n",
        cps, espKey, acKey, menuKey, version);

    // Migration: any config older than the current schema gets its
    // keybinds force-cleared. Catches the historical CapsLock→ESP default
    // that users had previously committed to disk.
    if (version < CURRENT_VERSION) {
        printf("[AC] Settings::Load migrating from v%d -> v%d (keybinds cleared)\n",
            version, CURRENT_VERSION);
        menuKey = 0;
        acKey   = 0;
        espKey  = 0;
        version = CURRENT_VERSION;
        Save();
    }
}
