#pragma once

// RAII wrapper around VMMDLL_SCATTER_HANDLE.
//
// Threading model (MemProcFS API):
//   VMM_HANDLE          — thread-safe, share the single global instance freely.
//   VMMDLL_SCATTER_HANDLE — NOT thread-safe; one ScatterRead per thread.
//
// Usage pattern:
//   ScatterRead sr(vmm, pid);           // create once per thread
//   sr.Add(addr, &field);               // queue typed reads
//   sr.AddRaw(addr, size, buf);         // queue raw byte reads
//   sr.Execute();                       // fire all queued reads
//   sr.Clear();                         // reset for next batch (same pid/flags)

class ScatterRead
{
public:
	ScatterRead(VMM_HANDLE hVMM, DWORD pid, DWORD flags = VMMDLL_FLAG_NOCACHE)
		: m_Pid(pid), m_Flags(flags)
		, m_Handle(VMMDLL_Scatter_Initialize(hVMM, pid, flags)) {}

	~ScatterRead() { VMMDLL_Scatter_CloseHandle(m_Handle); }

	// Non-copyable — scatter handle has exclusive ownership per thread.
	ScatterRead(const ScatterRead&) = delete;
	ScatterRead& operator=(const ScatterRead&) = delete;

	// Queue a typed single-value read. Buffer is written on Execute().
	template<typename T>
	void Add(uintptr_t addr, T* out)
	{
		VMMDLL_Scatter_PrepareEx(m_Handle, addr, sizeof(T), reinterpret_cast<PBYTE>(out), nullptr);
	}

	// Queue a raw byte-range read (bones, strings, bulk arrays).
	void AddRaw(uintptr_t addr, DWORD cb, void* out)
	{
		VMMDLL_Scatter_PrepareEx(m_Handle, addr, cb, reinterpret_cast<PBYTE>(out), nullptr);
	}

	// Fire all queued reads.
	void Execute() { VMMDLL_Scatter_ExecuteRead(m_Handle); }

	// Reset for the next batch — keeps the same pid and flags.
	void Clear() { VMMDLL_Scatter_Clear(m_Handle, m_Pid, m_Flags); }

private:
	DWORD                 m_Pid;
	DWORD                 m_Flags;
	VMMDLL_SCATTER_HANDLE m_Handle;
};
