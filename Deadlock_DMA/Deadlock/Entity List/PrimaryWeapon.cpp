#include "pch.h"

#include "Deadlock/Deadlock.h"
#include "EntityList.h"

namespace
{
	// CitadelAbilityVData::m_mapWeaponInfos is a 0x28-byte CUtlOrderedMap
	// (CUtlRBTree under the hood). The schema dump gives its size but not its
	// internals, and the two plausible layouts differ in both where the node
	// array pointer sits and how wide the tree-link indices are. Rather than
	// commit to one, probe every 8-byte slot of the header as a candidate node
	// array and every plausible link width, then keep the first bullet speed
	// that lands in a sane range. The primary-weapon ability carries a single
	// entry, so node 0 is the one we want either way.
	constexpr std::ptrdiff_t kMapHeaderSize = 0x28;

	// UtlRBTreeLinks_t is 4 indices wide: 8 bytes for uint16 indices, 16 for
	// uint32. The map's own Node_t then starts with the CGlobalSymbol key (8b)
	// before the inline CCitadelWeaponInfo value.
	constexpr std::ptrdiff_t kNodeValueOffsets[] = { 0x08 + 0x08, 0x10 + 0x08 };

	constexpr float kMinBulletSpeedHu = 1000.0f;
	constexpr float kMaxBulletSpeedHu = 200000.0f;
}

// Resolves the local pawn's primary-weapon base bullet speed from the ability
// VData chain. This is the static-template value: hero stat scaling and item
// %BulletSpeed bonuses (server-side) aren't included. Resets the cached value
// to 0 on any failure so the aim-assist priority chain falls back to default
// instead of using a stale previous-hero value across hero swaps/respawns.
void EntityList::RefreshPrimaryWeaponBulletSpeed(DMA_Connection* Conn, Process* Proc)
{
	// Logs only when the reason changes. This runs every 2s, and the useful
	// signal is "which step is it dying at", not how often. OffsetHealth used
	// to report a dead bullet speed; nothing does now, so it reports itself.
	static const char* s_LastReason = "";
	auto Bail = [](const char* why)
	{
		g_LocalBulletSpeed.store(0.0f, std::memory_order_relaxed);
		if (why != s_LastReason)
		{
			Log::Warn("[Wpn] bullet speed unresolved: {} ({} primary-weapon abilities)",
				why, m_PrimaryWeaponAbilityAddresses.size());
			s_LastReason = why;
		}
	};

	if (m_PrimaryWeaponAbilityAddresses.empty()) { Bail("no primary-weapon ability entities"); return; }

	uintptr_t LocalPawn = 0;
	{
		std::scoped_lock lk(Deadlock::m_LocalAddressMutex);
		LocalPawn = Deadlock::m_LocalPlayerPawnAddress;
	}
	if (!LocalPawn) { Bail("no local pawn"); return; }

	std::vector<uint32_t> OwnerHandles(m_PrimaryWeaponAbilityAddresses.size(), 0);
	m_sr->Clear();
	for (size_t i = 0; i < m_PrimaryWeaponAbilityAddresses.size(); ++i)
		m_sr->Add(m_PrimaryWeaponAbilityAddresses[i] + Offsets::C_BaseEntity::m_hOwnerEntity, &OwnerHandles[i]);
	m_sr->Execute();

	uintptr_t MyAbility = 0;
	for (size_t i = 0; i < m_PrimaryWeaponAbilityAddresses.size(); ++i)
	{
		if (GetEntityAddressFromHandle(CHandle{ OwnerHandles[i] }) == LocalPawn)
		{
			MyAbility = m_PrimaryWeaponAbilityAddresses[i];
			break;
		}
	}
	if (!MyAbility) { Bail("no ability owned by the local pawn"); return; }

	uintptr_t VDataPtr = 0;
	m_sr->Clear();
	m_sr->Add(MyAbility + Offsets::C_BaseEntity::m_pSubclassVData, &VDataPtr);
	m_sr->Execute();

	if (!VDataPtr) { Bail("m_pSubclassVData is null"); return; } // engine may not have populated it yet

	std::array<uint64_t, kMapHeaderSize / sizeof(uint64_t)> MapHeader{};
	m_sr->Clear();
	m_sr->AddRaw(VDataPtr + Offsets::CitadelAbilityVData::m_mapWeaponInfos,
		static_cast<DWORD>(MapHeader.size() * sizeof(uint64_t)), MapHeader.data());
	m_sr->Execute();

	// One scatter for every (candidate node array, link width) pair.
	struct Candidate { uintptr_t addr; float speed; };
	std::vector<Candidate> Candidates;
	Candidates.reserve(MapHeader.size() * std::size(kNodeValueOffsets));
	for (uint64_t Slot : MapHeader)
	{
		// Heap pointers only — skip the indices, counts and the less-func.
		if (Slot < 0x10000 || Slot > 0x7FFFFFFFFFFFull) continue;
		for (std::ptrdiff_t ValueOff : kNodeValueOffsets)
			Candidates.push_back({ static_cast<uintptr_t>(Slot) + ValueOff
				+ Offsets::CCitadelWeaponInfo::m_flBulletSpeed, 0.0f });
	}
	if (Candidates.empty()) { Bail("no heap pointers in the weapon-info map header"); return; }

	m_sr->Clear();
	for (auto& c : Candidates)
		m_sr->Add(c.addr, &c.speed);
	m_sr->Execute();

	float SpeedHu = 0.0f;
	for (const auto& c : Candidates)
	{
		if (c.speed >= kMinBulletSpeedHu && c.speed <= kMaxBulletSpeedHu) { SpeedHu = c.speed; break; }
	}

	if (SpeedHu == 0.0f) { Bail("no candidate slot held a plausible muzzle speed"); return; }

	if (g_LocalBulletSpeed.load(std::memory_order_relaxed) != SpeedHu)
	{
		g_LocalBulletSpeed.store(SpeedHu, std::memory_order_relaxed);
		s_LastReason = "";
		Log::Info("[Wpn] bulletSpd={:.0f}hu ({:.0f}m/s)",
			SpeedHu, SpeedHu / HammerUnitsPerMeter);
	}
}
