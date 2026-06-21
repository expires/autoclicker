#include <windows.h>
#include <tlhelp32.h>
#include <urlmon.h>
#include <wincrypt.h>
#include <conio.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "advapi32.lib")

static const char* DLL_URL_BASE =
    "https://github.com/expires/autoclicker/releases/download/dev/";

static const char* DLL_MODERN = "ac_1.21.11.dll";
static const char* DLL_LEGACY = "ac_1.8.9.dll";

#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_DIM     "\x1b[2m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_MAGENTA "\x1b[35m"

static void SetupConsole()
{

    SetConsoleOutputCP(CP_UTF8);

    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

static void PrintBanner()
{
    printf("\n" ANSI_CYAN ANSI_BOLD);
    printf(
        "                                   _ _      _             \n"
        "                                  | (_)    | |            \n"
        "  _ __ ___   __ _ _ __  _   _  ___| |_  ___| | _____ _ __ \n"
        " | '_ ` _ \\ / _` | '_ \\| | | |/ __| | |/ __| |/ / _ \\ '__|\n"
        " | | | | | | (_| | | | | |_| | (__| | | (__|   <  __/ |   \n"
        " |_| |_| |_|\\__,_|_| |_|\\__,_|\\___|_|_|\\___|_|\\_\\___|_|   \n"
        "                                                          \n"
    );
    printf(ANSI_RESET "\n");
}

static void PrintStep(const char* msg)
{
    printf("  " ANSI_YELLOW "[•]" ANSI_RESET " %s\n", msg);
}

static void PrintError(const char* msg)
{
    printf("  " ANSI_RED "[✗] %s" ANSI_RESET "\n", msg);
}

static void PrintSuccess()
{
    printf("\n");
    printf("  " ANSI_GREEN "╭────────────────────────────────────────╮" ANSI_RESET "\n");
    printf("  " ANSI_GREEN "│" ANSI_RESET "  " ANSI_BOLD ANSI_GREEN
           "✓  Successfully injected" ANSI_RESET
           "              " ANSI_GREEN "│" ANSI_RESET "\n");
    printf("  " ANSI_GREEN "╰────────────────────────────────────────╯" ANSI_RESET "\n");
    printf("\n");
}

static void WaitForKey()
{
    printf("  " ANSI_DIM "press any key to close..." ANSI_RESET);
    fflush(stdout);
    _getch();
    printf("\n");
}

struct GameWindow
{
    HWND    hwnd   = nullptr;
    DWORD   pid    = 0;
    bool    found  = false;
    bool    legacy = false;
    wchar_t title[256] = {};
};

static bool IsJavaProcess(DWORD pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    wchar_t path[MAX_PATH] = {};
    DWORD   sz = MAX_PATH;
    const bool ok = QueryFullProcessImageNameW(h, 0, path, &sz);
    CloseHandle(h);
    if (!ok) return false;

    const wchar_t* slash = wcsrchr(path, L'\\');
    const wchar_t* exe   = slash ? slash + 1 : path;
    return _wcsicmp(exe, L"javaw.exe") == 0 || _wcsicmp(exe, L"java.exe") == 0;
}

static BOOL CALLBACK EnumGameWindow(HWND hwnd, LPARAM lp)
{
    auto* g = reinterpret_cast<GameWindow*>(lp);

    if (!IsWindowVisible(hwnd)) return TRUE;

    wchar_t cls[64] = {};
    if (!GetClassNameW(hwnd, cls, 64)) return TRUE;

    const bool isGlfw  = _wcsicmp(cls, L"GLFW30") == 0;
    const bool isLwjgl = _wcsicmp(cls, L"LWJGL")  == 0;
    if (!isGlfw && !isLwjgl) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || !IsJavaProcess(pid)) return TRUE;

    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, 256);

    g->hwnd  = hwnd;
    g->pid   = pid;
    g->found = true;
    wcsncpy_s(g->title, title, _TRUNCATE);

    g->legacy = (wcsstr(title, L"1.8") != nullptr) || isLwjgl;
    return FALSE;
}

static GameWindow DetectGame()
{
    GameWindow g;
    EnumWindows(EnumGameWindow, reinterpret_cast<LPARAM>(&g));
    return g;
}

void InjectDLL(DWORD pid, const char* dllPath)
{
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return;
    void* mem = VirtualAllocEx(hProc, nullptr, strlen(dllPath) + 1,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProc, mem, dllPath, strlen(dllPath) + 1, nullptr);
    HANDLE thread = CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA"),
        mem, 0, nullptr);
    WaitForSingleObject(thread, INFINITE);
    VirtualFreeEx(hProc, mem, 0, MEM_RELEASE);
    CloseHandle(thread);
    CloseHandle(hProc);
}

static void CleanupStaleDlls(const char* tempDir)
{
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%sac_*.dll", tempDir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s%s", tempDir, fd.cFileName);
        DeleteFileA(full);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static void GenerateRandomHex(char* out, size_t outSize)
{
    BYTE bytes[8] = {};
    HCRYPTPROV prov;
    if (CryptAcquireContextA(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        CryptGenRandom(prov, sizeof(bytes), bytes);
        CryptReleaseContext(prov, 0);
    }
    else
    {
        DWORD t = GetTickCount() ^ GetCurrentProcessId();
        for (int i = 0; i < 8; ++i) bytes[i] = (BYTE)(t >> ((i & 3) * 8)) ^ (BYTE)(i * 31);
    }
    for (int i = 0; i < 8 && (size_t)(i * 2 + 3) < outSize; ++i)
        snprintf(out + i * 2, outSize - i * 2, "%02x", bytes[i]);
}

int main()
{
    SetupConsole();
    PrintBanner();

    char tempDir[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempDir) == 0)
    {
        PrintError("Failed to resolve temp directory");
        WaitForKey();
        return 1;
    }

    PrintStep("Cleaning stale files...");
    CleanupStaleDlls(tempDir);

    PrintStep("Detecting Minecraft version...");
    GameWindow game = DetectGame();
    if (!game.found)
    {
        PrintError("No Minecraft window found — launch Lunar Client first");
        WaitForKey();
        return 1;
    }

    const char* asset = game.legacy ? DLL_LEGACY : DLL_MODERN;
    printf("  " ANSI_GREEN "[•]" ANSI_RESET " Detected %s\n",
           game.legacy ? "1.8.9" : "1.21.x");

    char hash[24] = {};
    GenerateRandomHex(hash, sizeof(hash));
    char dllPath[MAX_PATH];
    snprintf(dllPath, sizeof(dllPath), "%sac_%s.dll", tempDir, hash);

    char url[512];
    snprintf(url, sizeof(url), "%s%s", DLL_URL_BASE, asset);

    PrintStep("Downloading...");
    HRESULT hr = URLDownloadToFileA(nullptr, url, dllPath, 0, nullptr);
    if (FAILED(hr))
    {
        PrintError("Download failed — check your internet connection");
        WaitForKey();
        return 1;
    }

    PrintStep("Injecting...");
    InjectDLL(game.pid, dllPath);

    MoveFileExA(dllPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

    PrintSuccess();
    WaitForKey();
    return 0;
}
