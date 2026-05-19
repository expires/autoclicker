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
    fprintf(f, "menuKey=%d\n",         menuKey);
    fprintf(f, "acKey=%d\n",           acKey);
    fprintf(f, "espKey=%d\n",          espKey);
    fprintf(f, "selfDestructKey=%d\n", selfDestructKey);
    fprintf(f, "version=%d\n",         version);

    fprintf(f, "macroCount=%d\n", macroCount);
    for (int i = 0; i < macroCount; ++i) {
        fprintf(f, "macro%d_name=%s\n",  i, macros[i].name);
        fprintf(f, "macro%d_delay=%d\n", i, macros[i].delay);
        fprintf(f, "macro%d_key=%d\n",   i, macros[i].key);
    }

    fprintf(f, "aimEnabled=%d\n",    aimEnabled    ? 1 : 0);
    fprintf(f, "aimClickOnly=%d\n",  aimClickOnly  ? 1 : 0);
    fprintf(f, "aimSpeedH=%d\n",     aimSpeedH);
    fprintf(f, "aimSpeedV=%d\n",     aimSpeedV);
    fprintf(f, "aimFov=%d\n",        aimFov);
    fprintf(f, "aimRange=%d\n",      aimRange);
    fprintf(f, "aimKey=%d\n",        aimKey);

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
        char*       valStr = eq + 1;
        // Strip trailing CR/LF so macro names don't pick them up.
        size_t vlen = strlen(valStr);
        while (vlen > 0 && (valStr[vlen-1] == '\n' || valStr[vlen-1] == '\r'))
            valStr[--vlen] = '\0';
        const int val = atoi(valStr);

        const std::string k = keyStr;
        if      (k == "acEnabled")    acEnabled    = (val != 0);
        else if (k == "breakBlocks")  breakBlocks  = (val != 0);
        else if (k == "cps")          cps          = val;
        else if (k == "espEnabled")   espEnabled   = (val != 0);
        else if (k == "drawBox")      drawBox      = (val != 0);
        else if (k == "drawName")     drawName     = (val != 0);
        else if (k == "drawDistance") drawDistance = (val != 0);
        else if (k == "menuKey")         menuKey         = val;
        else if (k == "acKey")           acKey           = val;
        else if (k == "espKey")          espKey          = val;
        else if (k == "selfDestructKey") selfDestructKey = val;
        else if (k == "version")      version      = val;
        else if (k == "macroCount")   macroCount   = val;
        else if (k == "aimEnabled")    aimEnabled    = (val != 0);
        else if (k == "aimClickOnly")  aimClickOnly  = (val != 0);
        else if (k == "aimSpeedH")     aimSpeedH     = val;
        else if (k == "aimSpeedV")     aimSpeedV     = val;
        else if (k == "aimFov")        aimFov        = val;
        else if (k == "aimRange")      aimRange      = val;
        else if (k == "aimKey")        aimKey        = val;
        else if (k.rfind("macro", 0) == 0) {
            // macro<i>_{name,delay,key}
            for (int i = 0; i < MAX_MACROS; ++i) {
                char buf[24];
                snprintf(buf, sizeof(buf), "macro%d_name", i);
                if (k == buf) { strncpy_s(macros[i].name, valStr, _TRUNCATE); break; }
                snprintf(buf, sizeof(buf), "macro%d_delay", i);
                if (k == buf) { macros[i].delay = val; break; }
                snprintf(buf, sizeof(buf), "macro%d_key", i);
                if (k == buf) { macros[i].key   = val; break; }
            }
        }
    }

    fclose(f);

    // Defensive: clamp keybinds to the valid VK range so a corrupt config
    // (or one written by a future build with a different schema) can never
    // feed an out-of-range vKey to GetAsyncKeyState.
    auto clampVK = [](int v) { return (v >= 0 && v <= 0xFE) ? v : 0; };
    menuKey         = clampVK(menuKey);
    acKey           = clampVK(acKey);
    espKey          = clampVK(espKey);
    selfDestructKey = clampVK(selfDestructKey);

    // Sanity-clamp CPS and bools.
    if (cps < 1)  cps = 1;
    if (cps > 50) cps = 50;

    if (macroCount < 0)          macroCount = 0;
    if (macroCount > MAX_MACROS) macroCount = MAX_MACROS;
    for (int i = 0; i < MAX_MACROS; ++i) {
        macros[i].key = clampVK(macros[i].key);
        if (macros[i].delay < 0)    macros[i].delay = 0;
        if (macros[i].delay > 2000) macros[i].delay = 2000;
    }

    // Aim-assist clamps. Sliders pin their own range in the UI, but a
    // hand-edited config can poke arbitrary integers in here — clamp before
    // they reach the aim thread (out-of-range speed → pixel overflow on the
    // SendInput delta, out-of-range FOV → wraparound on the cone check).
    if (aimSpeedH < 0)  aimSpeedH = 0;  if (aimSpeedH > 10) aimSpeedH = 10;
    if (aimSpeedV < 0)  aimSpeedV = 0;  if (aimSpeedV > 10) aimSpeedV = 10;
    if (aimFov    < 1)  aimFov    = 1;  if (aimFov    > 180) aimFov   = 180;
    if (aimRange  < 1)  aimRange  = 1;  if (aimRange  > 64)  aimRange = 64;
    aimKey = clampVK(aimKey);

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
