#include "pch.h"

#include "CHandle.h"

#include "Deadlock/Entity List/EntityList.h"

size_t CHandle::GetEntityListIndex() const
{
	return (Data & 0x7FFF) / MAX_ENTITIES;
}

size_t CHandle::GetEntityEntryIndex() const
{
	return (Data & 0x7FFF) % MAX_ENTITIES;
}

bool CHandle::IsValid() const
{
	// Index 0 is the world entity and 0x7FFF is the null-handle sentinel, so
	// neither is ever a real target. The old bound alone accepted both — a
	// handle read that failed and left 0 behind resolved to the world entity
	// and was latched as if it were a live pawn.
	const uint32_t Index = Data & 0x7FFF;
	return Index != 0 && Index < (MAX_ENTITIES * MAX_ENTITY_LISTS) - 1;
}