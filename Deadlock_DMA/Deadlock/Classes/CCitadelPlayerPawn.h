#pragma once
#include "pch.h"
#include "Deadlock/Engine/CHandle.h"
#include "Deadlock/Const/HeroSkeletonMap.hpp"
#include "CBaseEntity.h"
#include "CCitadelPlayerController.h"

constexpr int MAX_BONES      = 70;
constexpr int MAX_MODEL_PATH = 128;

// Stride of each bone entry in the game's bone array.
constexpr int BONE_STRIDE = 0x20;

class CCitadelPlayerPawn : public CBaseEntity
{
public:
	using CBaseEntity::CBaseEntity;

	Vector3   m_BonePositions[MAX_BONES]{ 0.0f };
	Vector3   m_Velocity{ 0.0f };
	uintptr_t m_BoneArrayAddress{ 0 };
	CHandle   m_hController{ 0 };
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

	void PrepareBoneRead(VMMDLL_SCATTER_HANDLE vmsh)
	{
		if (!m_BoneArrayAddress) return;
		VMMDLL_Scatter_PrepareEx(vmsh, m_BoneArrayAddress, m_BoneCount * BONE_STRIDE,
			reinterpret_cast<BYTE*>(m_BoneRawBuf), nullptr);
	}

public:
	void PrepareRead_1(VMMDLL_SCATTER_HANDLE vmsh)
	{
		CBaseEntity::PrepareRead_1(vmsh, false);

		uintptr_t hControllerPtr = m_EntityAddress + Offsets::CCitadelPlayerPawn::m_hController;
		VMMDLL_Scatter_PrepareEx(vmsh, hControllerPtr, sizeof(uint32_t), reinterpret_cast<BYTE*>(&m_hController.Data), nullptr);

		uintptr_t UnsecuredSoulsPtr = m_EntityAddress + Offsets::CCitadelPlayerPawn::m_nUnsecuredSouls;
		VMMDLL_Scatter_PrepareEx(vmsh, UnsecuredSoulsPtr, sizeof(int32_t), reinterpret_cast<BYTE*>(&m_UnsecuredSouls), nullptr);

		uintptr_t TotalUnspentSoulsPtr = m_EntityAddress + Offsets::CCitadelPlayerPawn::m_nTotalUnspentSouls;
		VMMDLL_Scatter_PrepareEx(vmsh, TotalUnspentSoulsPtr, sizeof(int32_t), reinterpret_cast<BYTE*>(&m_TotalUnspentSouls), nullptr);

		uintptr_t VelocityPtr = m_EntityAddress + Offsets::CCitadelPlayerPawn::m_vecVelocity;
		VMMDLL_Scatter_PrepareEx(vmsh, VelocityPtr, sizeof(Vector3), reinterpret_cast<BYTE*>(&m_Velocity), nullptr);
	}

	void PrepareRead_2(VMMDLL_SCATTER_HANDLE vmsh)
	{
		if (!m_GameSceneNodeAddress)
			SetInvalid();

		CBaseEntity::PrepareRead_2(vmsh);

		if (IsInvalid())
			return;

		uintptr_t BoneArrayPtrAddress = m_GameSceneNodeAddress + Offsets::CGameSceneNode::m_modelState + Offsets::CModelState::m_pBones;
		VMMDLL_Scatter_PrepareEx(vmsh, BoneArrayPtrAddress, sizeof(uintptr_t), reinterpret_cast<BYTE*>(&m_BoneArrayAddress), nullptr);

		// Read m_ModelName pointer (CUtlSymbolLarge = char* in Source 2)
		uintptr_t ModelNamePtrAddress = m_GameSceneNodeAddress + Offsets::CGameSceneNode::m_modelState + Offsets::CModelState::m_ModelName;
		VMMDLL_Scatter_PrepareEx(vmsh, ModelNamePtrAddress, sizeof(uintptr_t), reinterpret_cast<BYTE*>(&m_ModelNamePtr), nullptr);
	}

	void PrepareRead_3(VMMDLL_SCATTER_HANDLE vmsh)
	{
		if (IsInvalid())
			return;

		PrepareBoneRead(vmsh);

		// Read model path string from the pointer obtained in PrepareRead_2
		if (m_ModelNamePtr)
		{
			memset(m_ModelPathBuf, 0, sizeof(m_ModelPathBuf));
			VMMDLL_Scatter_PrepareEx(vmsh, m_ModelNamePtr, MAX_MODEL_PATH - 1,
				reinterpret_cast<BYTE*>(m_ModelPathBuf), nullptr);
		}
	}

	void QuickRead(VMMDLL_SCATTER_HANDLE vmsh)
	{
		if (IsInvalid())
			return;

		CBaseEntity::QuickRead(vmsh, false);

		uintptr_t VelocityPtr = m_EntityAddress + Offsets::CCitadelPlayerPawn::m_vecVelocity;
		VMMDLL_Scatter_PrepareEx(vmsh, VelocityPtr, sizeof(Vector3), reinterpret_cast<BYTE*>(&m_Velocity), nullptr);

		PrepareBoneRead(vmsh);
	}
};
