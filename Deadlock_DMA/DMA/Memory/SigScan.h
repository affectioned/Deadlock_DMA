#pragma once
#include "DMA/DMA.h"

// Returns all PIDs whose long process name contains 'name'.
std::vector<int> GetPidListFromName(DMA_Connection* Conn, std::string name);

// Scans [range_start, range_end) in process PID for a byte-pattern signature.
// Pattern format: "48 8B 05 ? ? ? ?" — '?' is a wildcard byte.
// Returns the address of the first match, or 0 on failure.
uint64_t FindSignature(DMA_Connection* Conn, const char* signature,
                       uint64_t range_start, uint64_t range_end, int PID);

// Returns true if 'addr' is mapped and readable in process PID.
// Used to validate resolved RIP-relative addresses before trusting them.
bool IsAddressReadable(DMA_Connection* Conn, uint64_t addr, DWORD PID);

// Single-value scatter read from an arbitrary PID.
template <typename T>
T ReadFromPID(DMA_Connection* Conn, uintptr_t Address, DWORD PID)
{
	T Value{};
	ScatterRead sr(Conn->GetHandle(), PID);
	sr.Add(Address, &Value);
	sr.Execute();
	return Value;
}
