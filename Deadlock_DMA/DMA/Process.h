#pragma once

namespace ConstStrings
{
	const std::string Game = "deadlock.exe";
	const std::string Client = "client.dll";
}

class Process
{
private:
	DWORD m_PID{ 0 };
	std::unordered_map<std::string, uintptr_t> m_Modules{};
	std::unordered_map<std::string, size_t>    m_ModuleSizes{};

public:
	bool GetProcessInfo(const std::string& ProcessName, DMA_Connection* Conn);
	const uintptr_t GetBaseAddress() const;
	const uintptr_t GetClientBase() const;
	const size_t    GetClientSize() const;
	const DWORD GetPID() const;
	const uintptr_t GetModuleAddress(const std::string& ModuleName);

private:
	bool PopulateModules(DMA_Connection* Conn);

public:
	template<typename T> inline T ReadMem(DMA_Connection* Conn, uintptr_t Address)
	{
		T Buffer{};
		ScatterRead sr(Conn->GetHandle(), m_PID);
		sr.Add(Address, &Buffer);
		sr.Execute();
		return Buffer;
	}
};
