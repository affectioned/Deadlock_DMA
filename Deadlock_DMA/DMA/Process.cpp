#include "pch.h"

#include "DMA.h"

#include "Process.h"

bool Process::GetProcessInfo(const std::string& ProcessName, DMA_Connection* Conn)
{

	std::println("Waiting for process {}..", ProcessName);

	m_PID = 0;

	while (true)
	{
		VMMDLL_PidGetFromName(Conn->GetHandle(), ProcessName.c_str(), &m_PID);

		if (m_PID)
		{
			std::println("Found process `{}` with PID {}", ProcessName, m_PID);
			PopulateModules(Conn);
			break;
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return true;
}

const uintptr_t Process::GetBaseAddress() const
{
	using namespace ConstStrings;
	return m_Modules.at(Game);
}

const uintptr_t Process::GetClientBase() const
{
	using namespace ConstStrings;
	return m_Modules.at(Client);
}

const size_t Process::GetClientSize() const
{
	using namespace ConstStrings;
	return m_ModuleSizes.at(Client);
}

const DWORD Process::GetPID() const
{
	return m_PID;
}

const uintptr_t Process::GetModuleAddress(const std::string& ModuleName)
{
	return m_Modules.at(ModuleName);
}

bool Process::PopulateModules(DMA_Connection* Conn)
{

	using namespace ConstStrings;

	auto Handle = Conn->GetHandle();

	while (!m_Modules[Game] || !m_Modules[Client])
	{
		if (!m_Modules[Game])
			m_Modules[Game] = VMMDLL_ProcessGetModuleBaseU(Handle, this->m_PID, Game.c_str());

		if (!m_Modules[Client])
			m_Modules[Client] = VMMDLL_ProcessGetModuleBaseU(Handle, this->m_PID, Client.c_str());

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	// Populate sizes for modules we care about
	for (const auto& name : { Game, Client })
	{
		PVMMDLL_MAP_MODULEENTRY info = nullptr;
		if (VMMDLL_Map_GetModuleFromNameU(Handle, m_PID, const_cast<LPSTR>(name.c_str()), &info, VMMDLL_MODULE_FLAG_NORMAL))
		{
			m_ModuleSizes[name] = info->cbImageSize;
			VMMDLL_MemFree(info);
		}
	}

	for (auto& [Name, Address] : m_Modules)
		std::println("Module `{}` at 0x{:X} size 0x{:X}", Name, Address, m_ModuleSizes[Name]);

	return false;
}
