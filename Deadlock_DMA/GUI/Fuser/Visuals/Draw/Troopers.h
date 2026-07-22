#pragma once

class Draw_Troopers
{
public:
	static void operator()();

public:
	static inline bool bMasterToggle{ false };
	static inline bool bHideFriendly{ false };

	// Sub-filters keyed off C_NPC_Trooper::m_Label (populated by SortEntityList):
	//   nullptr  → lane trooper
	//   "Walker" → npc_trooper_boss (guardian / walker)
	//   "Neutral"→ npc_trooper_neutral (jungle creep)
	// Split so the master "Draw Troopers" toggle doesn't take jungle creeps
	// down with the lane troopers.
	static inline bool bDrawLaneTroopers{ true };
	static inline bool bDrawWalkers{ true };
	static inline bool bDrawNeutrals{ true };
};
