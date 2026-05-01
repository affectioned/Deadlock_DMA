#include "pch.h"
#include "SigScan.h"

// Lookup table: ASCII character -> nibble value (0 for non-hex chars)
static const char hexdigits[] =
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\001\002\003\004\005\006\007\010\011\000\000\000\000\000\000"
"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000";

static uint8_t GetByte(const char* hex)
{
	return static_cast<uint8_t>((hexdigits[(unsigned char)hex[0]] << 4)
	                           | hexdigits[(unsigned char)hex[1]]);
}

std::vector<int> GetPidListFromName(DMA_Connection* Conn, std::string name)
{
	std::vector<int> list;

	PVMMDLL_PROCESS_INFORMATION process_info = nullptr;
	DWORD total_processes = 0;

	if (!VMMDLL_ProcessGetInformationAll(Conn->GetHandle(), &process_info, &total_processes))
	{
		Log::Warn("[SigScan]: Failed to enumerate processes while searching for \"{}\"", name);
		return list;
	}

	for (size_t i = 0; i < total_processes; i++)
	{
		if (strstr(process_info[i].szNameLong, name.c_str()))
			list.push_back(process_info[i].dwPID);
	}

	VMMDLL_MemFree(process_info);
	return list;
}

uint64_t FindSignature(DMA_Connection* Conn, const char* signature,
                       uint64_t range_start, uint64_t range_end, int PID)
{
	if (!signature || signature[0] == '\0' || range_start >= range_end)
		return 0;

	std::vector<uint8_t> buffer(range_end - range_start);
	if (!VMMDLL_MemReadEx(Conn->GetHandle(), PID, range_start,
	                      buffer.data(), static_cast<DWORD>(buffer.size()), 0, VMMDLL_FLAG_NOCACHE))
		return 0;

	const char* pat = signature;
	uint64_t first_match = 0;
	bool fullMatch = false;

	for (uint64_t i = range_start; i < range_end; i++)
	{
		if (*pat == '?' || buffer[i - range_start] == GetByte(pat))
		{
			if (!first_match)
				first_match = i;

			if (!pat[2]) { fullMatch = true; break; }

			pat += (*pat == '?') ? 2 : 3;
		}
		else
		{
			pat = signature;
			first_match = 0;
		}
	}

	return fullMatch ? first_match : 0;
}

bool IsAddressReadable(DMA_Connection* Conn, uint64_t addr, DWORD PID)
{
	if (!addr) return false;
	uint8_t probe;
	return VMMDLL_MemReadEx(Conn->GetHandle(), PID, addr, &probe, 1, nullptr, VMMDLL_FLAG_NOCACHE) != 0;
}
