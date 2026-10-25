#include "pch.h"

#include "DMA/DMA.h"
#include "Process.h"

bool Process::GetProcessInfo(const std::string& processName,
                              const std::vector<std::string>& moduleNames,
                              DMA_Connection* conn)
{
	Log::Info("Waiting for {}", processName);

	m_PID = 0;
	m_ProcessName = processName;

	while (true)
	{
		VMMDLL_PidGetFromName(conn->GetHandle(), processName.c_str(), &m_PID);

		if (m_PID)
		{
			Log::Info("{} pid={}", processName, m_PID);
			PopulateModules(moduleNames, conn);
			break;
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return true;
}

bool Process::IsAlive(DMA_Connection* conn) const
{
	if (!m_PID || m_ProcessName.empty()) return false;
	DWORD livePid = 0;
	// VMMDLL_PidGetFromName can return true-ish with a zero pid in some
	// states; guard against that too. Treat zero as not-alive.
	if (!VMMDLL_PidGetFromName(conn->GetHandle(), m_ProcessName.c_str(), &livePid) || !livePid)
		return false;
	// Catches "game restarted into a fresh PID": lookup resolves but to a
	// different PID than the one we attached to, so our caches are stale.
	return livePid == m_PID;
}

void Process::Reset()
{
	m_PID = 0;
	m_ProcessName.clear();
	m_Modules.clear();
	m_ModuleSizes.clear();
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
		for (const auto& name : names) {
			auto it = m_Modules.find(name);
			if (it == m_Modules.end() || !it->second) return false;
		}
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

	for (const auto& [name, addr] : m_Modules) {
		auto sit = m_ModuleSizes.find(name);
		Log::Info("Mod {} 0x{:X}+{:X}", name, addr,
		          sit != m_ModuleSizes.end() ? sit->second : 0);
	}

	return true;
}
