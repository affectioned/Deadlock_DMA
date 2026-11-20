#include "pch.h"

#include "DMA/DMA.h"
#include "DMA/DMA Thread.h"
#include "Process.h"

bool Process::GetProcessInfo(const std::string& processName,
                              const std::vector<std::string>& moduleNames,
                              DMA_Connection* conn)
{
	Log::Info("Waiting for {} — launch the game whenever, this loop sits here until it shows up", processName);

	m_PID = 0;
	m_ProcessName = processName;

	const auto start = std::chrono::steady_clock::now();
	auto lastHeartbeat = start;

	auto handle = conn->GetHandle();

	globals::g_attachPhase.store(globals::AttachPhase::WaitingForProcess, std::memory_order_release);

	while (true)
	{
		// DMA_Connection passes -norefresh to VMMDLL_Initialize, which freezes
		// the internal caches at startup. REFRESH_ALL forces a full rebuild
		// of the process map — MEDIUM wasn't enough in practice. This loop
		// only runs during the init wait, before the tick loop takes over,
		// so the per-tick cost is irrelevant.
		VMMDLL_ConfigSet(handle, VMMDLL_OPT_REFRESH_ALL, 1);

		VMMDLL_PidGetFromName(handle, processName.c_str(), &m_PID);

		if (m_PID)
		{
			Log::Info("{} pid={}", processName, m_PID);
			PopulateModules(moduleNames, conn);
			break;
		}

		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
		globals::g_attachElapsedSec.store((uint32_t)elapsed, std::memory_order_release);

		if (now - lastHeartbeat >= std::chrono::seconds(5))
		{
			Log::Info("Still waiting for {} ({}s)", processName, elapsed);
			lastHeartbeat = now;
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return true;
}

bool Process::IsAlive(DMA_Connection* conn) const
{
	if (!m_PID || m_ProcessName.empty()) return false;

	// Called every kAttachProbeInterval (2 s) by the DMA thread. A full
	// VMMDLL process refresh at that cadence over FPGA stalls the tick loop
	// for hundreds of ms per probe — the visuals freeze. The user's game
	// pid is overwhelmingly stable across their playtime, so a refresh every
	// 15 s is plenty: the worst case is a 15 s delay before a reattach after
	// the game restarts, which is still inside the overlay's own attach-
	// aware state.
	static auto sLastRefresh = std::chrono::steady_clock::now() - std::chrono::hours(1);
	auto now = std::chrono::steady_clock::now();
	if (now - sLastRefresh >= std::chrono::seconds(15))
	{
		VMMDLL_ConfigSet(conn->GetHandle(), VMMDLL_OPT_REFRESH_FREQ_MEDIUM, 1);
		sLastRefresh = now;
	}

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

	const auto start = std::chrono::steady_clock::now();
	auto lastHeartbeat = start;

	globals::g_attachPhase.store(globals::AttachPhase::WaitingForModules, std::memory_order_release);
	globals::g_attachElapsedSec.store(0, std::memory_order_release);

	while (!allResolved())
	{
		// Same -norefresh caveat as GetProcessInfo. SLOW isn't enough when
		// the game process came up *after* DMA init: the new PID's per-process
		// module table never gets walked and VMMDLL_ProcessGetModuleBaseU
		// keeps returning 0. REFRESH_ALL forces the module-map rebuild too.
		// Still init-path, so the blocking cost doesn't reach the tick loop.
		VMMDLL_ConfigSet(handle, VMMDLL_OPT_REFRESH_ALL, 1);

		// VMMDLL_Map_GetModuleU is what actually populates the per-process
		// module cache under the hood. Calling it here, even just to discard
		// the result, guarantees the lookups below hit a freshly-walked map.
		PVMMDLL_MAP_MODULE prime = nullptr;
		if (VMMDLL_Map_GetModuleU(handle, m_PID, &prime, VMMDLL_MODULE_FLAG_NORMAL) && prime)
			VMMDLL_MemFree(prime);

		for (const auto& name : names)
		{
			if (!m_Modules[name])
				m_Modules[name] = VMMDLL_ProcessGetModuleBaseU(handle, m_PID, name.c_str());
		}

		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
		globals::g_attachElapsedSec.store((uint32_t)elapsed, std::memory_order_release);

		if (now - lastHeartbeat >= std::chrono::seconds(5))
		{
			for (const auto& name : names) {
				if (!m_Modules[name])
					Log::Info("Still waiting for module {} ({}s)", name, elapsed);
			}
			lastHeartbeat = now;
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

	// Note: there is no "wait for last-page readability" loop here. The tail
	// of a PE image is often discardable or lazily paged even on a fully-
	// running DLL, so a probe at (base + size - 16) false-positives and
	// gets stuck. The real not-yet-mapped case that bites us is
	// schemasystem.dll loading mid-attach, and that's handled inside the
	// schema walker (SchemaWalker::FindSchemaSystemCandidates loops on
	// short reads with a VMMDLL refresh between tries).

	for (const auto& [name, addr] : m_Modules) {
		auto sit = m_ModuleSizes.find(name);
		Log::Info("Mod {} 0x{:X}+{:X}", name, addr,
		          sit != m_ModuleSizes.end() ? sit->second : 0);
	}

	return true;
}
