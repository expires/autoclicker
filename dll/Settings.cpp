#include "Settings.h"
#include <windows.h>
#include <shlobj.h>
#include <cctype>
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
    fprintf(f, "jitterEnabled=%d\n",  jitterEnabled ? 1 : 0);
    fprintf(f, "jitterStrength=%d\n", jitterStrength);
    fprintf(f, "espEnabled=%d\n",   espEnabled   ? 1 : 0);
    fprintf(f, "drawBox=%d\n",      drawBox      ? 1 : 0);
    fprintf(f, "drawName=%d\n",     drawName     ? 1 : 0);
    fprintf(f, "drawDistance=%d\n", drawDistance ? 1 : 0);
    fprintf(f, "drawHealth=%d\n",     drawHealth       ? 1 : 0);
    fprintf(f, "highlightFriends=%d\n", highlightFriends ? 1 : 0);
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

    fprintf(f, "leapEnabled=%d\n",    leapEnabled    ? 1 : 0);
    fprintf(f, "leapRequireAxe=%d\n", leapRequireAxe ? 1 : 0);
    fprintf(f, "leapInterval=%d\n",   leapInterval);
    fprintf(f, "leapKey=%d\n",        leapKey);

    fprintf(f, "autoAbilityEnabled=%d\n",      autoAbilityEnabled      ? 1 : 0);
    fprintf(f, "autoAbilityRequireSword=%d\n", autoAbilityRequireSword ? 1 : 0);
    fprintf(f, "autoAbilityDelay=%d\n",        autoAbilityDelay);
    fprintf(f, "autoAbilityCooldown=%d\n",     autoAbilityCooldown);
    fprintf(f, "autoAbilityKey=%d\n",          autoAbilityKey);

    fprintf(f, "friendKey=%d\n", friendKey);
    // Snapshot the friends list under lock — the friends-module / overlay
    // could be appending mid-Save. Worst case without the lock is a torn
    // string in mid-resize, which corrupts the on-disk list.
    {
        std::lock_guard<std::mutex> lk(friendsMutex);
        fprintf(f, "friendCount=%d\n", (int)friends.size());
        for (size_t i = 0; i < friends.size(); ++i)
            fprintf(f, "friend%zu=%s\n", i, friends[i].c_str());
    }

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
        else if (k == "jitterEnabled")  jitterEnabled  = (val != 0);
        else if (k == "jitterStrength") jitterStrength = val;
        else if (k == "espEnabled")   espEnabled   = (val != 0);
        else if (k == "drawBox")      drawBox      = (val != 0);
        else if (k == "drawName")     drawName     = (val != 0);
        else if (k == "drawDistance") drawDistance = (val != 0);
        else if (k == "drawHealth")       drawHealth       = (val != 0);
        else if (k == "highlightFriends") highlightFriends = (val != 0);
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
        else if (k == "leapEnabled")    leapEnabled    = (val != 0);
        else if (k == "leapRequireAxe") leapRequireAxe = (val != 0);
        else if (k == "leapInterval")   leapInterval   = val;
        else if (k == "leapKey")        leapKey        = val;
        else if (k == "autoAbilityEnabled")      autoAbilityEnabled      = (val != 0);
        else if (k == "autoAbilityRequireSword") autoAbilityRequireSword = (val != 0);
        else if (k == "autoAbilityDelay")        autoAbilityDelay        = val;
        else if (k == "autoAbilityCooldown")     autoAbilityCooldown     = val;
        else if (k == "autoAbilityKey")          autoAbilityKey          = val;
        else if (k == "friendKey")               friendKey               = val;
        else if (k == "friendCount") {
            // Reset before reading entries so a partial older list doesn't
            // leak stale tail names. The actual content arrives in subsequent
            // friend<i>=name lines handled below.
            std::lock_guard<std::mutex> lk(friendsMutex);
            friends.clear();
            int n = val;
            if (n < 0) n = 0;
            friends.reserve((size_t)n);
        }
        else if (k.rfind("friend", 0) == 0 && k.size() > 6 &&
                 isdigit((unsigned char)k[6])) {
            // friend<i>=<name>. We don't trust <i> to be contiguous (a hand-
            // edited config could skip indices), so just append in file order.
            std::string name = valStr;
            // Lowercase for case-insensitive matching — MC usernames are
            // case-insensitive on the server, and the ESP-side lookup will
            // also lowercase before comparing.
            for (char& c : name) c = (char)tolower((unsigned char)c);
            if (!name.empty()) {
                std::lock_guard<std::mutex> lk(friendsMutex);
                // Soft de-dup so a hand-edited config can't grow the list
                // forever with copies of the same name.
                bool exists = false;
                for (const auto& f : friends) if (f == name) { exists = true; break; }
                if (!exists) friends.push_back(std::move(name));
            }
        }
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

    if (jitterStrength < 0)  jitterStrength = 0;
    if (jitterStrength > 10) jitterStrength = 10;

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

    // Leap clamps. Interval floor of 50ms guards against accidentally
    // spamming right-clicks faster than MC's per-tick input handler can
    // service them (a high-rate spam looks more obviously botted in the
    // server's interaction packet stream anyway). 1000ms ceiling because
    // any slower than that and the cheat is uselessly sluggish.
    if (leapInterval < 50)   leapInterval = 50;
    if (leapInterval > 1000) leapInterval = 1000;
    leapKey = clampVK(leapKey);

    // Auto-ability clamps. Delay floor of 30ms keeps the right-click rate
    // below MC's per-tick interaction handler (which services use-item once
    // per 50ms tick anyway — finer pacing just wastes attempts). Cooldown
    // floor of 50ms because anything tighter is effectively no cooldown.
    if (autoAbilityDelay    < 30)   autoAbilityDelay    = 30;
    if (autoAbilityDelay    > 1000) autoAbilityDelay    = 1000;
    // Floor 0 — cooldown UI is a 0-30s slider; "0" is meaningful (means
    // "no extra gap beyond Delay") so the floor can't be a positive number.
    // Ceiling clamped to 30s (30000ms) to match the slider's max, so a
    // hand-edited config can't push the value off the slider track and
    // become un-adjustable from the UI.
    if (autoAbilityCooldown < 0)     autoAbilityCooldown = 0;
    if (autoAbilityCooldown > 30000) autoAbilityCooldown = 30000;
    autoAbilityKey = clampVK(autoAbilityKey);

    friendKey = clampVK(friendKey);

    // Migration: any config older than the current schema gets its
    // keybinds force-cleared. Catches the historical CapsLock→ESP default
    // that users had previously committed to disk; v3 also rolls the menu
    // key forward to VK_RSHIFT (0xA1) so existing users pick up the new
    // explicit default rather than the implicit VK_INSERT fallback.
    if (version < CURRENT_VERSION) {
        menuKey = 0xA1; // VK_RSHIFT
        acKey   = 0;
        espKey  = 0;
        version = CURRENT_VERSION;
        Save();
    }
}
