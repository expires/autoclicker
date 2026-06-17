#pragma once

#define AC_ENABLE_LOGGING

#ifdef AC_ENABLE_LOGGING

namespace Logger
{
    void Init();
    void Shutdown();
    void Writef(const char* fmt, ...);
}

#define AC_LOG(...) ::Logger::Writef(__VA_ARGS__)

#else

namespace Logger
{
    inline void Init() {}
    inline void Shutdown() {}
}

#define AC_LOG(...) ((void)0)

#endif
