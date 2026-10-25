#pragma once

class Process
{
private:
	DWORD       m_PID{ 0 };
	std::string m_ProcessName{};
	std::unordered_map<std::string, uintptr_t> m_Modules{};
	std::unordered_map<std::string, size_t>    m_ModuleSizes{};

public:
	// Blocks until the process is found, then resolves every module in moduleNames.
	bool GetProcessInfo(const std::string& processName,
	                    const std::vector<std::string>& moduleNames,
	                    DMA_Connection* conn);

	// Returns 0 if name was not in moduleNames at init time.
	uintptr_t GetModuleBase(const std::string& name) const;
	size_t    GetModuleSize(const std::string& name) const;
	DWORD     GetPID() const;

	// Liveness check for the attached PID. Rescans the process table for the
	// stored process name; returns true only if that lookup resolves to the
	// same PID we stored. Catches both "game exited" (lookup fails) and
	// "game restarted into a new PID" (lookup resolves to a different PID).
	// Cheap enough for a watchdog to call every couple of seconds.
	bool      IsAlive(DMA_Connection* conn) const;

	// Drop every per-process field — PID, module bases, module sizes, cached
	// process name. Call before reattaching so a new game instance's modules
	// aren't shadowed by stale cache entries from the previous one.
	void      Reset();

private:
	bool PopulateModules(const std::vector<std::string>& names, DMA_Connection* conn);

public:
	template<typename T>
	T ReadMem(DMA_Connection* conn, uintptr_t address)
	{
		T buf{};
		ScatterRead sr(conn->GetHandle(), m_PID);
		sr.Add(address, &buf);
		sr.Execute();
		return buf;
	}
};
