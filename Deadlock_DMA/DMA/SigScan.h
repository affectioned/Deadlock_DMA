#pragma once
#include "DMA.h"

// Returns all PIDs whose long process name contains 'name'.
std::vector<int> GetPidListFromName(DMA_Connection* Conn, std::string name);

// Scans [range_start, range_end) in process PID for a byte-pattern signature.
// Pattern format: "48 8B 05 ? ? ? ?" — '?' is a wildcard byte.
// Returns the address of the first match, or 0 on failure.
uint64_t FindSignature(DMA_Connection* Conn, const char* signature,
                       uint64_t range_start, uint64_t range_end, int PID);

// Single-value scatter read from an arbitrary PID.
template <typename T>
T ReadFromPID(DMA_Connection* Conn, uintptr_t Address, DWORD PID)
{
	T Value{};
	DWORD BytesRead = 0;

	auto vmsh = VMMDLL_Scatter_Initialize(Conn->GetHandle(), PID, VMMDLL_FLAG_NOCACHE);
	VMMDLL_Scatter_PrepareEx(vmsh, Address, sizeof(T), reinterpret_cast<BYTE*>(&Value), &BytesRead);
	VMMDLL_Scatter_ExecuteRead(vmsh);
	VMMDLL_Scatter_CloseHandle(vmsh);

	if (BytesRead != sizeof(T))
		return T{};

	return Value;
}
