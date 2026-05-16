#include <windows.h>
#include <tlhelp32.h>
#include <urlmon.h>
#include <wincrypt.h>
#include <conio.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "advapi32.lib")

// Rolling release URL — GitHub redirects /releases/latest/download/<file> to
// whichever asset is on the most recent release. Our CI publishes to a tag
// named 'latest' on every push to main, so this URL is always current.
static const char* DLL_URL =
    "https://github.com/expires/autoclicker/releases/latest/download/ac.dll";

// ─── ANSI escape codes ─────────────────────────────────────────────────────
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
    // UTF-8 so the box-drawing + checkmark characters render correctly.
    SetConsoleOutputCP(CP_UTF8);

    // Enable ANSI escape sequence processing (Windows 10 1607+).
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

// ─── Injection plumbing ────────────────────────────────────────────────────
DWORD GetProcessId(const wchar_t* procName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = { sizeof(entry) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &entry))
    {
        do
            if (!_wcsicmp(entry.szExeFile, procName))
                pid = entry.th32ProcessID;
        while (!pid && Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
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

    char hash[24] = {};
    GenerateRandomHex(hash, sizeof(hash));
    char dllPath[MAX_PATH];
    snprintf(dllPath, sizeof(dllPath), "%sac_%s.dll", tempDir, hash);

    PrintStep("Downloading DLL from GitHub...");
    HRESULT hr = URLDownloadToFileA(nullptr, DLL_URL, dllPath, 0, nullptr);
    if (FAILED(hr))
    {
        PrintError("Download failed — check your internet connection");
        WaitForKey();
        return 1;
    }

    PrintStep("Locating javaw.exe...");
    DWORD pid = GetProcessId(L"javaw.exe");
    if (!pid)
    {
        PrintError("javaw.exe not running — launch Lunar Client first");
        WaitForKey();
        return 1;
    }

    PrintStep("Injecting...");
    InjectDLL(pid, dllPath);

    // Belt-and-braces deletion fallback in case nothing sweeps the file later.
    MoveFileExA(dllPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

    PrintSuccess();
    WaitForKey();
    return 0;
}
