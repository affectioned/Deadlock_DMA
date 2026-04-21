#pragma once
#include <string>
#include <vector>

// Game-specific module names. Process::GetProcessInfo reads this list and
// resolves a base address + size for every entry. Add any extra DLLs here
// if you need their base addresses (e.g. "engine2.dll").
namespace GameModules
{
    inline constexpr const char* ProcessName = "deadlock.exe";
    inline constexpr const char* ClientDll   = "client.dll";

    inline std::vector<std::string> ModuleList()
    {
        return { ProcessName, ClientDll };
    }
}
