#include <windows.h>
#include <tlhelp32.h>
#include <urlmon.h>
#include <wincrypt.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "advapi32.lib")

// Rolling release URL — GitHub redirects /releases/latest/download/<file> to
// whichever asset is on the most recent release. Our CI publishes to a tag
// named 'latest' on every push to main, so this URL is always current.
static const char* DLL_URL =
    "https://github.com/expires/autoclicker/releases/latest/download/ac.dll";

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

// Best-effort sweep of stale DLL files dropped here by previous injector runs.
// Files locked by a still-running javaw will fail silently; the next run picks
// them up once the target process has exited.
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

// 16 hex chars of CSPRNG output — picks a unique filename so two concurrent
// runs can't collide on disk.
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
        // Fallback: tick count + PID, mixed.
        DWORD t = GetTickCount() ^ GetCurrentProcessId();
        for (int i = 0; i < 8; ++i) bytes[i] = (BYTE)(t >> ((i & 3) * 8)) ^ (BYTE)(i * 31);
    }
    for (int i = 0; i < 8 && (size_t)(i * 2 + 3) < outSize; ++i)
        snprintf(out + i * 2, outSize - i * 2, "%02x", bytes[i]);
}

int main()
{
    char tempDir[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempDir) == 0) return 1;

    // 1. Wipe whatever's lying around from prior runs.
    CleanupStaleDlls(tempDir);

    // 2. Generate a fresh unique path.
    char hash[24] = {};
    GenerateRandomHex(hash, sizeof(hash));
    char dllPath[MAX_PATH];
    snprintf(dllPath, sizeof(dllPath), "%sac_%s.dll", tempDir, hash);

    // 3. Pull the DLL from the rolling release. URLDownloadToFile handles
    //    HTTPS + cross-host redirects (GitHub redirects releases/latest/download
    //    to a hashed objects.githubusercontent.com URL).
    HRESULT hr = URLDownloadToFileA(nullptr, DLL_URL, dllPath, 0, nullptr);
    if (FAILED(hr)) return 1;

    // 4. Find javaw and inject. If javaw isn't running we still leave the DLL
    //    in place — next run's cleanup pass will sweep it.
    DWORD pid = GetProcessId(L"javaw.exe");
    if (pid) InjectDLL(pid, dllPath);

    // 5. Belt-and-braces: schedule the file for delete on next reboot, so even
    //    if no future injector run sweeps it, it doesn't accumulate forever.
    MoveFileExA(dllPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

    return 0;
}
