#pragma once
#include "C_BaseEntity.h"

class C_NPC_Trooper : public C_BaseEntity
{
public:
	using C_BaseEntity::C_BaseEntity;

public:
	void PrepareRead_1(ScatterRead& sr)
	{
		C_BaseEntity::PrepareRead_1(sr, true, m_Label != nullptr);
	}

	void PrepareRead_2(ScatterRead& sr)
	{
		if (m_CurrentHealth < 1)
			SetInvalid();

		if (IsInvalid())
			return;

		C_BaseEntity::PrepareRead_2(sr);
	}

	void QuickRead(ScatterRead& sr)
	{
		if (m_CurrentHealth < 1)
			SetInvalid();

		if (IsInvalid())
			return;

		C_BaseEntity::QuickRead(sr, true);
	}
};