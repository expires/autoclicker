#pragma once

#ifdef MNC_ENABLE_LOGGING

namespace Logger
{
    void Init();
    void Shutdown();
    void Writef(const char* fmt, ...);
}

#define LOG(...) ::Logger::Writef(__VA_ARGS__)

#else

namespace Logger
{
    inline void Init() {}
    inline void Shutdown() {}
}

#define LOG(...) ((void)0)

#endif
