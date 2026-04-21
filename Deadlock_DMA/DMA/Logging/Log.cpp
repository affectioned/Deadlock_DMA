#include "pch.h"
#include <fstream>
#include "Log.h"

static std::ofstream s_LogFile;
static std::mutex    s_Mutex;

void Log::Init(const std::wstring& logPath)
{
    s_LogFile.open(logPath, std::ios::out | std::ios::trunc);
}

void Log::Write(std::string_view prefix, std::string msg)
{
    std::lock_guard lock(s_Mutex);
    std::println("{}{}", prefix, msg);
    if (s_LogFile.is_open())
    {
        s_LogFile << prefix << msg << '\n';
        s_LogFile.flush();
    }
}
