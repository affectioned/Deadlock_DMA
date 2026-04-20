#pragma once
#include "pch.h"
#include "Deadlock/Engine/CHandle.h"
#include "Deadlock/Const/HeroSkeletonMap.hpp"
#include "C_BaseEntity.h"
#include "CCitadelPlayerController.h"

constexpr int MAX_BONES      = 70;
constexpr int MAX_MODEL_PATH = 128;

// Stride of each bone entry in the game's bone array.
constexpr int BONE_STRIDE = 0x20;

class C_CitadelPlayerPawn : public C_BaseEntity
{
public:
	using C_BaseEntity::C_BaseEntity;

	Vector3   m_BonePositions[MAX_BONES]{ 0.0f };
	Vector3   m_Velocity{ 0.0f };
	Vector3   m_EyeAngles{ 0.0f };
	uintptr_t m_BoneArrayAddress{ 0 };
	CHandle   m_hController{ 0 };
	float     m_flRespawnTime{ 0.0f };
	int32_t   m_TotalUnspentSouls{ 0 };
	int32_t   m_UnsecuredSouls{ 0 };

	// Model path: two-hop read (SceneNode + ModelState + ModelName → char*)
	uintptr_t m_ModelNamePtr{ 0 };
	char      m_ModelPathBuf[MAX_MODEL_PATH]{ 0 };

	// Cached pointer into g_HeroModelData — valid after CacheBoneData(), null if unknown hero.
	const ModelBoneData* m_pBoneData{ nullptr };

	// Highest bone index we need + 1. Derived from bone data; falls back to MAX_BONES
	// until the first CacheBoneData() call resolves the hero.
	int m_BoneCount{ MAX_BONES };

	std::string_view GetModelPath() const { return { m_ModelPathBuf }; }

	// Call once after the model path string has been read (end of FullPawnRefresh).
	void CacheBoneData()
	{
		m_pBoneData = GetHeroBoneData(GetModelPath());
		if (!m_pBoneData)
			return;

		int maxIdx = 0;
		for (const auto& [name, idx] : m_pBoneData->ids)
			maxIdx = std::max(maxIdx, idx);
		for (int s = 0; s < kHitboxSlotCount; s++)
			for (int16_t idx : m_pBoneData->slotBones[s])
				maxIdx = std::max(maxIdx, (int)idx);

		m_BoneCount = std::min(maxIdx + 1, MAX_BONES);
	}

	// Copy Vector3 positions out of the raw stride-0x20 bulk read buffer.
	void ExtractBones()
	{
		for (int i = 0; i < m_BoneCount; i++)
			memcpy(&m_BonePositions[i], m_BoneRawBuf + (i * BONE_STRIDE), sizeof(Vector3));
	}

private:
	// Single contiguous buffer for one scatter read covering all bone slots.
	uint8_t m_BoneRawBuf[MAX_BONES * BONE_STRIDE]{ 0 };

	void PrepareBoneRead(ScatterRead& sr)
	{
		if (!m_BoneArrayAddress) return;
		sr.AddRaw(m_BoneArrayAddress, m_BoneCount * BONE_STRIDE, m_BoneRawBuf);
	}

public:
	void PrepareRead_1(ScatterRead& sr)
	{
		C_BaseEntity::PrepareRead_1(sr, false);

		uintptr_t hControllerPtr = m_EntityAddress + Offsets::C_CitadelPlayerPawn::m_hController;
		sr.Add(hControllerPtr, &m_hController.Data);

		uintptr_t UnsecuredSoulsPtr = m_EntityAddress + Offsets::C_CitadelPlayerPawn::m_nUnsecuredSouls;
		sr.Add(UnsecuredSoulsPtr, &m_UnsecuredSouls);

		uintptr_t TotalUnspentSoulsPtr = m_EntityAddress + Offsets::C_CitadelPlayerPawn::m_nTotalUnspentSouls;
		sr.Add(TotalUnspentSoulsPtr, &m_TotalUnspentSouls);

		uintptr_t VelocityPtr = m_EntityAddress + Offsets::C_CitadelPlayerPawn::m_vecVelocity;
		sr.Add(VelocityPtr, &m_Velocity);

		uintptr_t EyeAnglesPtr = m_EntityAddress + Offsets::C_CitadelPlayerPawn::m_angEyeAngles;
		sr.Add(EyeAnglesPtr, &m_EyeAngles);

		uintptr_t RespawnTimePtr = m_EntityAddress + Offsets::C_CitadelPlayerPawn::m_flRespawnTime;
		sr.Add(RespawnTimePtr, &m_flRespawnTime);
	}

	void PrepareRead_2(ScatterRead& sr)
	{
		if (!m_GameSceneNodeAddress)
			SetInvalid();

		C_BaseEntity::PrepareRead_2(sr);

		if (IsInvalid())
			return;

		uintptr_t BoneArrayPtrAddress = m_GameSceneNodeAddress + Offsets::CGameSceneNode::m_modelState + Offsets::CModelState::m_pBones;
		sr.Add(BoneArrayPtrAddress, &m_BoneArrayAddress);

		// Read m_ModelName pointer (CUtlSymbolLarge = char* in Source 2)
		uintptr_t ModelNamePtrAddress = m_GameSceneNodeAddress + Offsets::CGameSceneNode::m_modelState + Offsets::CModelState::m_ModelName;
		sr.Add(ModelNamePtrAddress, &m_ModelNamePtr);
	}

	void PrepareRead_3(ScatterRead& sr)
	{
		if (IsInvalid())
			return;

		PrepareBoneRead(sr);

		// Read model path string from the pointer obtained in PrepareRead_2
		if (m_ModelNamePtr)
		{
			memset(m_ModelPathBuf, 0, sizeof(m_ModelPathBuf));
			sr.AddRaw(m_ModelNamePtr, MAX_MODEL_PATH - 1, m_ModelPathBuf);
		}
	}

	void QuickRead(ScatterRead& sr)
	{
		if (IsInvalid())
			return;

		C_BaseEntity::QuickRead(sr, false);

		uintptr_t VelocityPtr = m_EntityAddress + Offsets::C_CitadelPlayerPawn::m_vecVelocity;
		sr.Add(VelocityPtr, &m_Velocity);

		PrepareBoneRead(sr);
	}
};
