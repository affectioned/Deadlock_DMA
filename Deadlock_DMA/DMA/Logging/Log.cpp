#include "pch.h"
#include <fstream>
#include <chrono>
#include <format>
#include "Log.h"

static std::ofstream s_LogFile;
static std::mutex    s_Mutex;

static std::string timestamp()
{
    auto now  = std::chrono::system_clock::now();
    auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto tt   = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};
    localtime_s(&bt, &tt);
    return std::format("{:02}:{:02}:{:02}.{:03}", bt.tm_hour, bt.tm_min, bt.tm_sec, ms.count());
}

void Log::Init(const std::wstring& logPath)
{
    s_LogFile.open(logPath, std::ios::out | std::ios::trunc);
}

void Log::Write(std::string_view prefix, std::string msg)
{
    std::lock_guard lock(s_Mutex);
    auto ts = timestamp();
    std::println("{} {}{}", ts, prefix, msg);
    if (s_LogFile.is_open())
    {
        s_LogFile << ts << ' ' << prefix << msg << '\n';
        s_LogFile.flush();
    }
}
