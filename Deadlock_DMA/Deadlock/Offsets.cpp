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

	// GameEntitySystem — "48 8B 0D ? ? ? ? 8B FD C1 EF"
	try
	{
		uint64_t hit = FindSignature(Conn, "48 8B 0D ? ? ? ? 8B FD C1 EF", clientBase, clientEnd, pid);
		if (!hit) throw std::runtime_error("sig not found");
		Offsets::GameEntitySystem = ResolveRIP(Conn, pid, clientBase, hit, 3, 7);
		std::println("[+] GameEntitySystem Offset: 0x{:X}", Offsets::GameEntitySystem);
	}
	catch (...)
	{
		Offsets::GameEntitySystem = 0x31887F8;
		std::println("[!] GameEntitySystem sig failed, using fallback RVA: 0x{:X}", Offsets::GameEntitySystem);
	}

	// LocalController — "48 3B 35 ? ? ? ? 75 ? 48 C7 05"
	try
	{
		uint64_t hit = FindSignature(Conn, "48 3B 35 ? ? ? ? 75 ? 48 C7 05", clientBase, clientEnd, pid);
		if (!hit) throw std::runtime_error("sig not found");
		Offsets::LocalController = ResolveRIP(Conn, pid, clientBase, hit, 3, 7);
		std::println("[+] LocalController Offset: 0x{:X}", Offsets::LocalController);
	}
	catch (...)
	{
		Offsets::LocalController = 0x3708D90;
		std::println("[!] LocalController sig failed, using fallback RVA: 0x{:X}", Offsets::LocalController);
	}

	// ViewMatrix — "F3 0F 10 05 ? ? ? ? F3 0F 59 01"
	// CViewRender pointer (dwViewRender) = 0x37647B0; ViewMatrix is typically 0x400 bytes before it.
	try
	{
		uint64_t hit = FindSignature(Conn, "F3 0F 10 05 ? ? ? ? F3 0F 59 01", clientBase, clientEnd, pid);
		if (!hit) throw std::runtime_error("sig not found");
		Offsets::ViewMatrix = ResolveRIP(Conn, pid, clientBase, hit, 4, 8);
		// Reject false positives that land in the code section (< 32 MB into client.dll)
		if (Offsets::ViewMatrix < 0x2000000)
			throw std::runtime_error("ViewMatrix resolved into code section");
		std::println("[+] ViewMatrix Offset: 0x{:X}", Offsets::ViewMatrix);
	}
	catch (...)
	{
		Offsets::ViewMatrix = 0x373DDB0;
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
		Offsets::PredictionPtr = 0x2E95F98;
		std::println("[!] PredictionPtr sig failed, using fallback RVA: 0x{:X}", Offsets::PredictionPtr);
	}

	DbgPrintln("All offsets resolved.");
	return true;
}
