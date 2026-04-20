#pragma once
#include "Deadlock/Engine/Vector3.h"
#include "Deadlock/Const/ETeam.h"

class C_BaseEntity
{
public:
	uintptr_t m_EntityAddress{ 0 };
	Vector3 m_Position{ 0.0f };
	uintptr_t m_GameSceneNodeAddress{ 0 };
	int32_t m_MaxHealth{ 0 };
	int32_t m_CurrentHealth{ 0 };
	uint8_t m_Flags{ 0 };
	ETeam m_TeamNum{ 0 };
	uint8_t m_Dormant{ 0 };
	const char* m_Label{ nullptr };

public:
	const bool IsInvalid() const { return m_Flags & 0x1; }
	void SetInvalid() { m_Flags |= 0x1; }
	const bool IsFriendly() const;
	const bool IsLocalPlayer() const;
	const float DistanceFromLocalPlayer(bool bInMeters = 0) const;
	const bool IsDormant() const { return m_Dormant; }

	C_BaseEntity(uintptr_t _EntityAddress) : m_EntityAddress(_EntityAddress) {};

public:
	void PrepareRead_1(ScatterRead& sr, bool bReadHealth = true, bool bReadMaxHealth = false)
	{
		if (!m_EntityAddress)
			SetInvalid();

		if (IsInvalid())
			return;

		uintptr_t GameSceneNodePtr = m_EntityAddress + Offsets::C_BaseEntity::m_pGameSceneNode;
		sr.Add(GameSceneNodePtr, &m_GameSceneNodeAddress);

		uintptr_t TeamNumPtr = m_EntityAddress + Offsets::C_BaseEntity::m_iTeamNum;
		sr.Add(TeamNumPtr, &m_TeamNum);

		if (bReadHealth)
		{
			if (bReadMaxHealth)
			{
				uintptr_t MaxHealthPtr = m_EntityAddress + Offsets::C_BaseEntity::m_iMaxHealth;
				sr.Add(MaxHealthPtr, &m_MaxHealth);
			}
			uintptr_t HealthPtr = m_EntityAddress + Offsets::C_BaseEntity::m_iHealth;
			sr.Add(HealthPtr, &m_CurrentHealth);
		}
	}

	void PrepareRead_2(ScatterRead& sr)
	{
		if (!m_GameSceneNodeAddress)
			SetInvalid();

		if (IsInvalid())
			return;

		uintptr_t PositionAddress = m_GameSceneNodeAddress + Offsets::CGameSceneNode::m_vecAbsOrigin;
		sr.Add(PositionAddress, &m_Position);

		uintptr_t DormantAddress = m_GameSceneNodeAddress + Offsets::CGameSceneNode::m_bDormant;
		sr.Add(DormantAddress, &m_Dormant);
	}

	void QuickRead(ScatterRead& sr, bool bReadHealth = true)
	{
		if (IsInvalid())
			return;

		if (bReadHealth)
		{
			uintptr_t HealthPtr = m_EntityAddress + Offsets::C_BaseEntity::m_iHealth;
			sr.Add(HealthPtr, &m_CurrentHealth);
		}

		uintptr_t PositionAddress = m_GameSceneNodeAddress + Offsets::CGameSceneNode::m_vecAbsOrigin;
		sr.Add(PositionAddress, &m_Position);

		uintptr_t DormantAddress = m_GameSceneNodeAddress + Offsets::CGameSceneNode::m_bDormant;
		sr.Add(DormantAddress, &m_Dormant);
	}

	bool operator==(const C_BaseEntity& Other) const
	{
		return m_EntityAddress == Other.m_EntityAddress;
	}
};
