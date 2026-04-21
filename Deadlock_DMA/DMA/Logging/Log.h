#pragma once
#include <format>
#include <string>

class Log
{
public:
    // Call once at startup before any logging. Opens the log file next to the exe.
    static void Init(const std::wstring& logPath);

    template<typename... Args>
    static void Info(std::format_string<Args...> fmt, Args&&... args)
    {
        Write("[INFO] ", std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Warn(std::format_string<Args...> fmt, Args&&... args)
    {
        Write("[WARN] ", std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Error(std::format_string<Args...> fmt, Args&&... args)
    {
        Write("[ERR ] ", std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Debug(std::format_string<Args...> fmt, Args&&... args)
    {
        Write("[DBG ] ", std::format(fmt, std::forward<Args>(args)...));
    }

private:
    static void Write(std::string_view prefix, std::string msg);
};

#ifdef DBGPRINT
#define DbgLog(...) Log::Debug(__VA_ARGS__)
#else
#define DbgLog(...)
#endif
