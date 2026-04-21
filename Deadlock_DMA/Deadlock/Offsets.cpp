#include "pch.h"

#include "Offsets.h"
#include "Deadlock.h"
#include "GameModules.h"

// Resolve a RIP-relative MOV/LEA reference and return the module-relative offset.
// sigHit   – absolute address of the first byte of the matched instruction
// dispOff  – byte offset within the instruction where the 32-bit displacement lives
// instrSz  – total size of the instruction (RIP advances past it before applying disp)
// Returns 0 if the resolved address is not readable (false-positive match).
static ptrdiff_t ResolveRIP(DMA_Connection* Conn, DWORD pid,
                             uintptr_t clientBase, uint64_t sigHit,
                             int dispOff, int instrSz)
{
	int32_t disp = ReadFromPID<int32_t>(Conn, sigHit + dispOff, pid);
	uintptr_t absAddr = static_cast<uintptr_t>(sigHit) + instrSz + disp;
	if (!IsAddressReadable(Conn, absAddr, pid))
		return 0;
	return static_cast<ptrdiff_t>(absAddr - clientBase);
}

static void ResolveOffset(DMA_Connection* Conn, DWORD pid, uintptr_t clientBase, uintptr_t clientEnd,
                           const char* name, ptrdiff_t& target, ptrdiff_t fallback,
                           const char* sig, int dispOff, int instrSz)
{
	uint64_t hit = FindSignature(Conn, sig, clientBase, clientEnd, pid);
	ptrdiff_t offset = hit ? ResolveRIP(Conn, pid, clientBase, hit, dispOff, instrSz) : 0;

	if (offset)
	{
		target = offset;
		std::println("[+] {} Offset: 0x{:X}", name, target);
	}
	else
	{
		target = fallback;
		std::println("[!] {} sig failed, using fallback RVA: 0x{:X}", name, target);
	}
}

bool Offsets::ResolveOffsets(DMA_Connection* Conn)
{
	DWORD pid = Deadlock::Proc().GetPID();
	uintptr_t clientBase = Deadlock::Proc().GetModuleBase(GameModules::ClientDll);
	uintptr_t clientEnd  = clientBase + Deadlock::Proc().GetModuleSize(GameModules::ClientDll);

	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"GameEntitySystem", Offsets::GameEntitySystem, 0x31887F8,
		"48 8B 0D ? ? ? ? 8B FD C1 EF", 3, 7);

	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"LocalController", Offsets::LocalController, 0x3708D90,
		"48 3B 35 ? ? ? ? 75 ? 48 C7 05", 3, 7);

	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"ViewMatrix", Offsets::ViewMatrix, 0x373DDB0,
		"F3 0F 10 05 ? ? ? ? F3 0F 59 01", 4, 8);

	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"CPrediction", Offsets::Prediction, 0x2E95F98,
		"48 8B 05 ? ? ? ? 38 58", 3, 7);

	DbgPrintln("All offsets resolved.");
	return true;
}
