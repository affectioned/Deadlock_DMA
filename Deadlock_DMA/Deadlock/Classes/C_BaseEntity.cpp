#include "pch.h"
#include "C_BaseEntity.h"
#include "Deadlock/Entity List/EntityList.h"

const bool C_BaseEntity::IsFriendly() const
{
	if (EntityList::m_LocalControllerIndex < 0)
		return false;

	auto& LocalController = EntityList::m_PlayerControllers[EntityList::m_LocalControllerIndex];

	return m_TeamNum == LocalController.m_TeamNum;
}

const bool C_BaseEntity::IsLocalPlayer() const
{
	std::scoped_lock lock(Deadlock::m_LocalAddressMutex);
	return m_EntityAddress == Deadlock::m_LocalPlayerControllerAddress || m_EntityAddress == Deadlock::m_LocalPlayerPawnAddress;
}

constexpr uint32_t HammerUnitsPerMeter = 52;
const float C_BaseEntity::DistanceFromLocalPlayer(bool bInMeters) const
{
	if (EntityList::m_LocalPawnIndex < 0)
		return 0.0f;

	auto& LocalPawn = EntityList::m_PlayerPawns[EntityList::m_LocalPawnIndex];

	auto Distance = m_Position.Distance(LocalPawn.m_Position);

	if (bInMeters)
		return Distance / HammerUnitsPerMeter; 

	return Distance;
}