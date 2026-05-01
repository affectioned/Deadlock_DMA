#include "pch.h"

#include "EntityList.h"

void EntityList::PrintPlayerControllerAddresses()
{
	for (auto& Addr : m_PlayerController_Addresses)
		Log::Info("PlayerController: 0x{:X}", Addr);
}

void EntityList::PrintPlayerControllers()
{
	if (m_PlayerControllers.empty())
	{
		Log::Info("No PlayerControllers found.");
		return;
	}

	for (auto& PC : m_PlayerControllers)
	{
		if (PC.IsInvalid()) continue;

		auto PawnIt = std::find(m_PlayerPawns.begin(), m_PlayerPawns.end(), EntityList::GetEntityAddressFromHandle(PC.m_hHeroPawn));

		if (PawnIt == std::end(m_PlayerPawns)) continue;

		if (PawnIt->IsInvalid()) continue;

		Log::Info("Player found! hPawn: {0:d}, hController {1:d}  Pawn @ {2:.0f} with {3:d} hp", PC.m_hHeroPawn.GetEntityEntryIndex(), PawnIt->m_hController.GetEntityEntryIndex(), PawnIt->m_BonePositions[0].z, PC.m_CurrentHealth);
	}
}

void EntityList::PrintPlayerPawns()
{
	for (auto& Pawn : m_PlayerPawns)
		Log::Info("PlayerPawn: 0x{0:X} | GameSceneNode: {1:X}", Pawn.m_EntityAddress, Pawn.m_GameSceneNodeAddress);
}

void EntityList::PrintClassMap()
{
	for (auto& [name, addr] : m_EntityClassMap)
		Log::Info("Class: {} | Address: 0x{:X}", name, addr);
}
