#include <windows.h>
#include <tlhelp32.h>

DWORD GetProcessId(const wchar_t *procName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = {sizeof(entry)};
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

void InjectDLL(DWORD pid, const char *dllPath)
{
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc)
        return;
    void *mem = VirtualAllocEx(hProc, nullptr, strlen(dllPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProc, mem, dllPath, strlen(dllPath) + 1, nullptr);
    HANDLE thread = CreateRemoteThread(hProc, nullptr, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA"), mem, 0, nullptr);
    WaitForSingleObject(thread, INFINITE);
    VirtualFreeEx(hProc, mem, 0, MEM_RELEASE);
    CloseHandle(thread);
    CloseHandle(hProc);
}

int main()
{
    char dllPath[MAX_PATH];
    GetModuleFileNameA(nullptr, dllPath, MAX_PATH);
    *strrchr(dllPath, '\\') = '\0';
    strcat(dllPath, "\\ac.dll");
    DWORD pid = GetProcessId(L"javaw.exe");
    if (pid)
        InjectDLL(pid, dllPath);
    return 0;
}
