#include "Settings.h"
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    if (path.empty()) return;
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "w") != 0 || !f) return;

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

    fclose(f);
}

void Settings::Load()
{
    const std::string path = ConfigPath();
    if (path.empty()) return;
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "r") != 0 || !f) return;

    // Assume legacy / pre-versioned until proven otherwise; a missing
    // `version=` line then triggers the migration block below.
    version = 0;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Hand-rolled key=value parser. sscanf_s with `%[^=]` was failing
        // silently on x64 — its size argument is variadic and width
        // mismatches between sizeof() and rsize_t made the parser return 0
        // matches without telling us. strchr is unambiguous.
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* keyStr = line;
        const int   val    = atoi(eq + 1);

        const std::string k = keyStr;
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

    // Defensive: clamp keybinds to the valid VK range so a corrupt config
    // (or one written by a future build with a different schema) can never
    // feed an out-of-range vKey to GetAsyncKeyState.
    auto clampVK = [](int v) { return (v >= 0 && v <= 0xFE) ? v : 0; };
    menuKey = clampVK(menuKey);
    acKey   = clampVK(acKey);
    espKey  = clampVK(espKey);

    // Sanity-clamp CPS and bools.
    if (cps < 1)  cps = 1;
    if (cps > 50) cps = 50;

    // Migration: any config older than the current schema gets its
    // keybinds force-cleared. Catches the historical CapsLock→ESP default
    // that users had previously committed to disk.
    if (version < CURRENT_VERSION) {
        menuKey = 0;
        acKey   = 0;
        espKey  = 0;
        version = CURRENT_VERSION;
        Save();
    }
}
