#include "Logger.h"

#ifdef MNC_ENABLE_LOGGING

#include <windows.h>
#include <shlobj.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

namespace Logger
{
    static std::mutex s_mu;
    static FILE*      s_file = nullptr;
    static LPTOP_LEVEL_EXCEPTION_FILTER s_prevFilter = nullptr;

    static std::string ResolvePath()
    {
        char path[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
            return {};
        const std::string base = std::string(path) + "\\manuclicker";
        const std::string dir  = base + "\\logs";
        CreateDirectoryA(base.c_str(), nullptr);
        CreateDirectoryA(dir.c_str(),  nullptr);
        return dir + "\\latest.log";
    }

    static void WriteRaw(const char* prefix, const char* msg)
    {
        if (!s_file) return;
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(s_file, "[%02d:%02d:%02d.%03d][t%lu]%s %s\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            GetCurrentThreadId(), prefix, msg);
        fflush(s_file);
    }

    static LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* ep)
    {
        if (s_file && ep && ep->ExceptionRecord)
        {
            void* addr = ep->ExceptionRecord->ExceptionAddress;
            char line[512];
            snprintf(line, sizeof(line), "code=0x%08lX addr=%p",
                (unsigned long)ep->ExceptionRecord->ExceptionCode, addr);
            WriteRaw(" [FATAL]", line);

            HMODULE mod = nullptr;
            if (GetModuleHandleExA(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    (LPCSTR)addr, &mod) && mod)
            {
                char name[MAX_PATH] = {};
                GetModuleFileNameA(mod, name, MAX_PATH);
                char info[MAX_PATH + 128];
                snprintf(info, sizeof(info), "%s  base=%p  offset=0x%IX",
                    name, (void*)mod,
                    (uintptr_t)addr - (uintptr_t)mod);
                WriteRaw(" [FATAL]", info);
            }
            fflush(s_file);
        }
        if (s_prevFilter) return s_prevFilter(ep);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void Init()
    {
        std::lock_guard<std::mutex> lk(s_mu);
        if (s_file) return;
        const std::string p = ResolvePath();
        if (p.empty()) return;
        if (fopen_s(&s_file, p.c_str(), "a") != 0) s_file = nullptr;
        if (!s_file) return;

        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(s_file,
            "\n========== session %04d-%02d-%02d %02d:%02d:%02d ==========\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        fflush(s_file);

        s_prevFilter = SetUnhandledExceptionFilter(UnhandledFilter);
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> lk(s_mu);
        if (s_prevFilter) { SetUnhandledExceptionFilter(s_prevFilter); s_prevFilter = nullptr; }
        if (s_file) { fflush(s_file); fclose(s_file); s_file = nullptr; }
    }

    void Writef(const char* fmt, ...)
    {
        std::lock_guard<std::mutex> lk(s_mu);
        if (!s_file) return;
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(s_file, "[%02d:%02d:%02d.%03d][t%lu] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            GetCurrentThreadId());
        va_list args;
        va_start(args, fmt);
        vfprintf(s_file, fmt, args);
        va_end(args);
        fputc('\n', s_file);
        fflush(s_file);
    }
}

#endif
