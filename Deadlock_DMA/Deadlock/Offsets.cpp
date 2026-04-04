#include "pch.h"

#include "Offsets.h"

#include "Deadlock.h"

// Resolve a RIP-relative MOV/LEA reference and return the module-relative offset.
// sigHit   – absolute address of the first byte of the matched instruction
// dispOff  – byte offset within the instruction where the 32-bit displacement lives
// instrSz  – total size of the instruction (RIP advances past it before applying disp)
static ptrdiff_t ResolveRIP(DMA_Connection* Conn, DWORD pid,
                             uintptr_t clientBase, uint64_t sigHit,
                             int dispOff, int instrSz)
{
	int32_t disp = ReadFromPID<int32_t>(Conn, sigHit + dispOff, pid);
	uintptr_t absAddr = static_cast<uintptr_t>(sigHit) + instrSz + disp;
	return static_cast<ptrdiff_t>(absAddr - clientBase);
}

bool Offsets::ResolveOffsets(DMA_Connection* Conn)
{
	DWORD pid = Deadlock::Proc().GetPID();
	uintptr_t clientBase = Deadlock::Proc().GetClientBase();
	uintptr_t clientEnd  = clientBase + Deadlock::Proc().GetClientSize();

	// GameEntitySystem — "48 8B 0D ? ? ? ? 8B FD"
	try
	{
		uint64_t hit = FindSignature(Conn, "48 8B 0D ? ? ? ? 8B FD", clientBase, clientEnd, pid);
		if (!hit) throw std::runtime_error("sig not found");
		Offsets::GameEntitySystem = ResolveRIP(Conn, pid, clientBase, hit, 3, 7);
		std::println("[+] GameEntitySystem Offset: 0x{:X}", Offsets::GameEntitySystem);
	}
	catch (...)
	{
		Offsets::GameEntitySystem = 0x3845eb8;
		std::println("[!] GameEntitySystem sig failed, using fallback RVA: 0x{:X}", Offsets::GameEntitySystem);
	}

	// LocalController — "48 3B 35 ? ? ? ?"
	try
	{
		uint64_t hit = FindSignature(Conn, "48 3B 35 ? ? ? ?", clientBase, clientEnd, pid);
		if (!hit) throw std::runtime_error("sig not found");
		Offsets::LocalController = ResolveRIP(Conn, pid, clientBase, hit, 3, 7);
		std::println("[+] LocalController Offset: 0x{:X}", Offsets::LocalController);
	}
	catch (...)
	{
		Offsets::LocalController = 0x36f2fa0;
		std::println("[!] LocalController sig failed, using fallback RVA: 0x{:X}", Offsets::LocalController);
	}

	// ViewMatrix — "F3 0F 10 05 ? ? ? ? F3 0F 59 01"
	try
	{
		uint64_t hit = FindSignature(Conn, "F3 0F 10 05 ? ? ? ? F3 0F 59 01", clientBase, clientEnd, pid);
		if (!hit) throw std::runtime_error("sig not found");
		Offsets::ViewMatrix = ResolveRIP(Conn, pid, clientBase, hit, 4, 8);
		std::println("[+] ViewMatrix Offset: 0x{:X}", Offsets::ViewMatrix);
	}
	catch (...)
	{
		Offsets::ViewMatrix = 0x3728010;
		std::println("[!] ViewMatrix sig failed, using fallback RVA: 0x{:X}", Offsets::ViewMatrix);
	}

	// PredictionPtr — "48 8B 05 ? ? ? ? 38 58"
	// Fallback: dwViewRender (0x3728410) — confirm if this is the right mapping
	try
	{
		uint64_t hit = FindSignature(Conn, "48 8B 05 ? ? ? ? 38 58", clientBase, clientEnd, pid);
		if (!hit) throw std::runtime_error("sig not found");
		Offsets::PredictionPtr = ResolveRIP(Conn, pid, clientBase, hit, 3, 7);
		std::println("[+] PredictionPtr Offset: 0x{:X}", Offsets::PredictionPtr);
	}
	catch (...)
	{
		Offsets::PredictionPtr = 0x3728410;
		std::println("[!] PredictionPtr sig failed, using fallback RVA: 0x{:X}", Offsets::PredictionPtr);
	}

	DbgPrintln("All offsets resolved.");
	return true;
}
