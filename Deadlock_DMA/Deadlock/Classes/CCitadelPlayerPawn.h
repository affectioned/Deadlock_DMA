#pragma once
#include "pch.h"
#include "Deadlock/Engine/CHandle.h"
#include "CBaseEntity.h"
#include "CCitadelPlayerController.h"

constexpr int MAX_BONES   = 600;
constexpr int MAX_MODEL_PATH = 128;

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

	std::string_view GetModelPath() const { return { m_ModelPathBuf }; }

	// Called after VMMDLL_Scatter_Execute to unpack positions from the raw bone block.
	void ExtractBonePositions()
	{
		for (int i = 0; i < MAX_BONES; i++)
			memcpy(&m_BonePositions[i], m_BoneBlock + i * BONE_STRIDE, sizeof(Vector3));
	}

private:
	// Source 2 lays out each bone entry as 0x20 bytes: position (Vector3) first, then rotation.
	// Reading the whole array in one scatter call is far more efficient than 600 individual reads.
	static constexpr int BONE_STRIDE = 0x20;
	uint8_t m_BoneBlock[MAX_BONES * BONE_STRIDE]{ 0 };

	void PrepareBoneRead(VMMDLL_SCATTER_HANDLE vmsh)
	{
		if (!m_BoneArrayAddress) return;

		// One read covers all bones — position is extracted afterwards in ExtractBonePositions().
		VMMDLL_Scatter_PrepareEx(vmsh, m_BoneArrayAddress, sizeof(m_BoneBlock),
			reinterpret_cast<BYTE*>(m_BoneBlock), nullptr);
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
