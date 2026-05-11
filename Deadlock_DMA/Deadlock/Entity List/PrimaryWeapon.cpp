#include "pch.h"

#include "Deadlock/Deadlock.h"
#include "EntityList.h"

// Resolves the local pawn's primary-weapon base bullet speed from the ability
// VData chain. This is the static-template value: hero stat scaling and item
// %BulletSpeed bonuses (server-side) aren't included. Resets the cached value
// to 0 on any failure so the Aimbot priority chain falls back to default
// instead of using a stale previous-hero value across hero swaps/respawns.
void EntityList::RefreshPrimaryWeaponBulletSpeed(DMA_Connection* Conn, Process* Proc)
{
	auto ClearOnFailure = [] { g_LocalBulletSpeed.store(0.0f, std::memory_order_relaxed); };

	if (m_PrimaryWeaponAbilityAddresses.empty()) { ClearOnFailure(); return; }

	uintptr_t LocalPawn = 0;
	{
		std::scoped_lock lk(Deadlock::m_LocalAddressMutex);
		LocalPawn = Deadlock::m_LocalPlayerPawnAddress;
	}
	if (!LocalPawn) { ClearOnFailure(); return; }

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
	if (!MyAbility) { ClearOnFailure(); return; }

	uintptr_t VDataPtr = 0;
	m_sr->Clear();
	m_sr->Add(MyAbility + Offsets::C_BaseEntity::m_pSubclassVData, &VDataPtr);
	m_sr->Execute();

	if (!VDataPtr) { ClearOnFailure(); return; } // engine hasn't populated subclass data yet

	float SpeedHu = 0.0f;
	m_sr->Clear();
	m_sr->Add(VDataPtr + Offsets::CitadelAbilityVData::m_flBulletSpeed, &SpeedHu);
	m_sr->Execute();

	if (SpeedHu < 1000.0f || SpeedHu > 200000.0f) { ClearOnFailure(); return; }

	if (g_LocalBulletSpeed.load(std::memory_order_relaxed) != SpeedHu)
	{
		g_LocalBulletSpeed.store(SpeedHu, std::memory_order_relaxed);
		Log::Info("[PrimaryWeapon] base bullet speed = {:.0f} hu/s ({:.0f} m/s)",
			SpeedHu, SpeedHu / HammerUnitsPerMeter);
	}
}
