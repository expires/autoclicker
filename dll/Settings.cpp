#include "Settings.h"
#include <windows.h>
#include <shlobj.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#pragma comment(lib, "shell32.lib")

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
    fprintf(f, "teamsByColor=%d\n",     teamsByColor ? 1 : 0);
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

    fprintf(f, "autoblockEnabled=%d\n",      autoblockEnabled      ? 1 : 0);
    fprintf(f, "autoblockRequireSword=%d\n", autoblockRequireSword ? 1 : 0);
    fprintf(f, "autoblockDelay=%d\n",        autoblockDelay);
    fprintf(f, "autoblockCooldown=%d\n",     autoblockCooldown);
    fprintf(f, "autoblockKey=%d\n",          autoblockKey);

    fprintf(f, "friendKey=%d\n", friendKey);

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

    version = 0;

    char line[256];
    while (fgets(line, sizeof(line), f)) {

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* keyStr = line;
        char*       valStr = eq + 1;

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
        else if (k == "teamsByColor")     teamsByColor     = (val != 0);
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
        else if (k == "autoblockEnabled")      autoblockEnabled      = (val != 0);
        else if (k == "autoblockRequireSword") autoblockRequireSword = (val != 0);
        else if (k == "autoblockDelay")        autoblockDelay        = val;
        else if (k == "autoblockCooldown")     autoblockCooldown     = val;
        else if (k == "autoblockKey")          autoblockKey          = val;
        else if (k == "friendKey")               friendKey               = val;
        else if (k == "friendCount") {

            std::lock_guard<std::mutex> lk(friendsMutex);
            friends.clear();
            int n = val;
            if (n < 0) n = 0;
            friends.reserve((size_t)n);
        }
        else if (k.rfind("friend", 0) == 0 && k.size() > 6 &&
                 isdigit((unsigned char)k[6])) {

            std::string name = valStr;

            for (char& c : name) c = (char)tolower((unsigned char)c);
            if (!name.empty()) {
                std::lock_guard<std::mutex> lk(friendsMutex);

                bool exists = false;
                for (const auto& f : friends) if (f == name) { exists = true; break; }
                if (!exists) friends.push_back(std::move(name));
            }
        }
        else if (k.rfind("macro", 0) == 0) {

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

    auto clampVK = [](int v) { return (v >= 0 && v <= 0xFE) ? v : 0; };
    menuKey         = clampVK(menuKey);
    acKey           = clampVK(acKey);
    espKey          = clampVK(espKey);
    selfDestructKey = clampVK(selfDestructKey);

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

    if (aimSpeedH < 0)  aimSpeedH = 0;  if (aimSpeedH > 10) aimSpeedH = 10;
    if (aimSpeedV < 0)  aimSpeedV = 0;  if (aimSpeedV > 10) aimSpeedV = 10;
    if (aimFov    < 1)  aimFov    = 1;  if (aimFov    > 180) aimFov   = 180;
    if (aimRange  < 1)  aimRange  = 1;  if (aimRange  > 64)  aimRange = 64;
    aimKey = clampVK(aimKey);

    if (autoblockDelay    < 30)   autoblockDelay    = 30;
    if (autoblockDelay    > 1000) autoblockDelay    = 1000;

    if (autoblockCooldown < 0)     autoblockCooldown = 0;
    if (autoblockCooldown > 30000) autoblockCooldown = 30000;
    autoblockKey = clampVK(autoblockKey);

    friendKey = clampVK(friendKey);

    if (version < CURRENT_VERSION) {
        menuKey = 0xA1;
        acKey   = 0;
        espKey  = 0;
        version = CURRENT_VERSION;
        Save();
    }
}
