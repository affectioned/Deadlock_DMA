#include "pch.h"

#include "DMA/DMA.h"
#include "Process.h"

bool Process::GetProcessInfo(const std::string& processName,
                              const std::vector<std::string>& moduleNames,
                              DMA_Connection* conn)
{
	Log::Info("Waiting for process {}..", processName);

	m_PID = 0;

	while (true)
	{
		VMMDLL_PidGetFromName(conn->GetHandle(), processName.c_str(), &m_PID);

		if (m_PID)
		{
			Log::Info("Found process `{}` with PID {}", processName, m_PID);
			PopulateModules(moduleNames, conn);
			break;
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return true;
}

uintptr_t Process::GetModuleBase(const std::string& name) const
{
	auto it = m_Modules.find(name);
	return it != m_Modules.end() ? it->second : 0;
}

size_t Process::GetModuleSize(const std::string& name) const
{
	auto it = m_ModuleSizes.find(name);
	return it != m_ModuleSizes.end() ? it->second : 0;
}

DWORD Process::GetPID() const
{
	return m_PID;
}

bool Process::PopulateModules(const std::vector<std::string>& names, DMA_Connection* conn)
{
	auto handle = conn->GetHandle();

	auto allResolved = [&]
	{
		for (const auto& name : names)
			if (!m_Modules[name]) return false;
		return true;
	};

	while (!allResolved())
	{
		for (const auto& name : names)
		{
			if (!m_Modules[name])
				m_Modules[name] = VMMDLL_ProcessGetModuleBaseU(handle, m_PID, name.c_str());
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	for (const auto& name : names)
	{
		PVMMDLL_MAP_MODULEENTRY info = nullptr;
		if (VMMDLL_Map_GetModuleFromNameU(handle, m_PID, const_cast<LPSTR>(name.c_str()), &info, VMMDLL_MODULE_FLAG_NORMAL))
		{
			m_ModuleSizes[name] = info->cbImageSize;
			VMMDLL_MemFree(info);
		}
	}

	for (const auto& [name, addr] : m_Modules)
		Log::Info("Module `{}` at 0x{:X} size 0x{:X}", name, addr, m_ModuleSizes[name]);

	return true;
}
