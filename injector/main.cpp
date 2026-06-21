#include <windows.h>
#include <tlhelp32.h>
#include <urlmon.h>
#include <wincrypt.h>
#include <conio.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uuid.lib")

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

static const char* BANNER[] = {
    "                                   _ _      _             ",
    "                                  | (_)    | |            ",
    "  _ __ ___   __ _ _ __  _   _  ___| |_  ___| | _____ _ __ ",
    " | '_ ` _ \\ / _` | '_ \\| | | |/ __| | |/ __| |/ / _ \\ '__|",
    " | | | | | | (_| | | | | |_| | (__| | | (__|   <  __/ |   ",
    " |_| |_| |_|\\__,_|_| |_|\\__,_|\\___|_|_|\\___|_|\\_\\___|_|   ",
};
static const int BANNER_LINES = (int)(sizeof(BANNER) / sizeof(BANNER[0]));

static HANDLE g_out      = nullptr;
static int    g_width    = 80;
static SHORT  g_stageRow = 0;
static SHORT  g_barRow   = 0;
static SHORT  g_footRow  = 0;

static int ConsoleWidth()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_out, &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    return 80;
}

static int ConsoleHeight()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_out, &csbi))
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return 25;
}

static void MoveTo(SHORT row)
{
    COORD c; c.X = 0; c.Y = row;
    SetConsoleCursorPosition(g_out, c);
}

static void ShowCursor(bool show)
{
    CONSOLE_CURSOR_INFO ci;
    if (GetConsoleCursorInfo(g_out, &ci))
    {
        ci.bVisible = show ? TRUE : FALSE;
        SetConsoleCursorInfo(g_out, &ci);
    }
}

static void CenterLine(SHORT row, const char* color, const char* text, int visibleLen)
{
    int pad = (g_width - visibleLen) / 2;
    if (pad < 0) pad = 0;
    MoveTo(row);
    printf("\x1b[2K%*s%s%s%s", pad, "", color ? color : "", text, ANSI_RESET);
    fflush(stdout);
}

static void SetupConsole()
{
    SetConsoleOutputCP(CP_UTF8);
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD mode = 0;
    if (GetConsoleMode(g_out, &mode))
        SetConsoleMode(g_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

static void UiInit()
{
    g_width = ConsoleWidth();
    int height = ConsoleHeight();

    int blockW = 0;
    for (int i = 0; i < BANNER_LINES; ++i)
    {
        int len = (int)strlen(BANNER[i]);
        if (len > blockW) blockW = len;
    }

    const int total = BANNER_LINES + 4;
    SHORT top = (SHORT)((height - total) / 2);
    if (top < 0) top = 0;

    int bannerPad = (g_width - blockW) / 2;
    if (bannerPad < 0) bannerPad = 0;

    ShowCursor(false);
    printf("\x1b[2J\x1b[3J");
    fflush(stdout);

    for (int i = 0; i < BANNER_LINES; ++i)
    {
        MoveTo((SHORT)(top + i));
        printf("%*s%s%s%s", bannerPad, "", ANSI_CYAN ANSI_BOLD, BANNER[i], ANSI_RESET);
    }
    fflush(stdout);

    g_stageRow = (SHORT)(top + BANNER_LINES + 1);
    g_barRow   = (SHORT)(g_stageRow + 1);
    g_footRow  = (SHORT)(g_barRow + 2);
}

static void UiStage(const char* color, const char* text)
{
    CenterLine(g_stageRow, color, text, (int)strlen(text));
}

static void UiBar(double frac, const char* color)
{
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;

    const int cells = 32;
    int filled = (int)(frac * cells + 0.5);
    if (filled > cells) filled = cells;

    int pct = (int)(frac * 100.0 + 0.5);
    int visible = cells + 2 + 5;
    int pad = (g_width - visible) / 2;
    if (pad < 0) pad = 0;

    MoveTo(g_barRow);
    printf("\x1b[2K%*s%s[", pad, "", color ? color : ANSI_CYAN);
    for (int i = 0; i < filled; ++i)        fputs("\xe2\x96\x88", stdout);
    for (int i = filled; i < cells; ++i)    fputs(ANSI_DIM "\xe2\x96\x91" ANSI_RESET, stdout);
    printf("%s] %3d%%%s", color ? color : ANSI_CYAN, pct, ANSI_RESET);
    fflush(stdout);
}

static void UiFooter(const char* color, const char* text)
{
    CenterLine(g_footRow, color, text, (int)strlen(text));
}

static void WaitForKey()
{
    UiFooter(ANSI_DIM, "press any key to close...");
    _getch();
    MoveTo((SHORT)(g_footRow + 2));
    ShowCursor(true);
    printf("\n");
}

class DownloadProgress : public IBindStatusCallback
{
public:
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IBindStatusCallback)
        {
            *ppv = static_cast<IBindStatusCallback*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override  { return 1; }
    STDMETHOD_(ULONG, Release)() override { return 1; }

    STDMETHOD(OnStartBinding)(DWORD, IBinding*) override { return S_OK; }
    STDMETHOD(GetPriority)(LONG*) override { return E_NOTIMPL; }
    STDMETHOD(OnLowResource)(DWORD) override { return S_OK; }
    STDMETHOD(OnStopBinding)(HRESULT, LPCWSTR) override { return S_OK; }
    STDMETHOD(GetBindInfo)(DWORD*, BINDINFO*) override { return S_OK; }
    STDMETHOD(OnDataAvailable)(DWORD, DWORD, FORMATETC*, STGMEDIUM*) override { return S_OK; }
    STDMETHOD(OnObjectAvailable)(REFIID, IUnknown*) override { return S_OK; }

    STDMETHOD(OnProgress)(ULONG ulProgress, ULONG ulProgressMax, ULONG, LPCWSTR) override
    {
        double f = ulProgressMax ? (double)ulProgress / (double)ulProgressMax : 0.0;
        UiStage(ANSI_CYAN, "Downloading");
        UiBar(0.45 + f * 0.45, ANSI_CYAN);
        return S_OK;
    }
};

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

static int Fail(const char* msg)
{
    UiStage(ANSI_RED, msg);
    UiBar(1.0, ANSI_RED);
    WaitForKey();
    return 1;
}

int main()
{
    SetupConsole();
    UiInit();

    UiStage(ANSI_CYAN, "Preparing");
    UiBar(0.05, ANSI_CYAN);

    char tempDir[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempDir) == 0)
        return Fail("Failed to resolve temp directory");

    UiStage(ANSI_CYAN, "Cleaning stale files");
    UiBar(0.15, ANSI_CYAN);
    CleanupStaleDlls(tempDir);

    UiStage(ANSI_CYAN, "Detecting Minecraft");
    UiBar(0.30, ANSI_CYAN);
    GameWindow game = DetectGame();
    if (!game.found)
        return Fail("No Minecraft window found - launch Lunar Client first");

    const char* asset = game.legacy ? DLL_LEGACY : DLL_MODERN;
    UiStage(ANSI_GREEN, game.legacy ? "Detected 1.8.9" : "Detected 1.21.x");
    UiBar(0.40, ANSI_CYAN);

    char hash[24] = {};
    GenerateRandomHex(hash, sizeof(hash));
    char dllPath[MAX_PATH];
    snprintf(dllPath, sizeof(dllPath), "%sac_%s.dll", tempDir, hash);

    char url[512];
    snprintf(url, sizeof(url), "%s%s", DLL_URL_BASE, asset);

    UiStage(ANSI_CYAN, "Downloading");
    UiBar(0.45, ANSI_CYAN);
    DownloadProgress cb;
    HRESULT hr = URLDownloadToFileA(nullptr, url, dllPath, 0, &cb);
    if (FAILED(hr))
        return Fail("Download failed - check your internet connection");

    UiStage(ANSI_CYAN, "Injecting");
    UiBar(0.92, ANSI_CYAN);
    InjectDLL(game.pid, dllPath);

    MoveFileExA(dllPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

    UiStage(ANSI_GREEN ANSI_BOLD, "Injected successfully");
    UiBar(1.0, ANSI_GREEN);
    WaitForKey();
    return 0;
}
