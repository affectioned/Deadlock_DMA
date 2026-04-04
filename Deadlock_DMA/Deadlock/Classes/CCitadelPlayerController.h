#pragma once
#include "CBaseEntity.h"
#include "Deadlock/Engine/CHandle.h"
#include "Deadlock/Const/HeroMap.hpp"
#include "Deadlock/Const/HeroEnum.hpp"

class CCitadelPlayerController : public CBaseEntity
{
public:
	using CBaseEntity::CBaseEntity;

	CHandle  m_hPawn{ 0 };
	int32_t  m_CurrentLevel{ 0 };
	int32_t  m_MaxHealth{ 0 };
	int32_t  m_TotalSouls{ 0 };
	HeroId   m_HeroID{ 0 };
	bool     m_bUltimateTrained{ false };
	float    m_flUltimateCooldownStart{ 0.f };
	float    m_flUltimateCooldownEnd{ 0.f };

public:
	const bool IsDead() const { return m_CurrentHealth <= 0; }
	const std::string_view GetHeroName() const
	{
		auto it = HeroNames::HeroNameMap.find(m_HeroID);

		if (it != HeroNames::HeroNameMap.end())
			return it->second;

		return "Unknown";
	}

public:
	void PrepareRead_1(VMMDLL_SCATTER_HANDLE vmsh)
	{
		CBaseEntity::PrepareRead_1(vmsh, false);

		if (IsInvalid())
			return;

		uintptr_t PawnHandleAddress = m_EntityAddress + Offsets::CCitadelPlayerController::m_hPawn;
		VMMDLL_Scatter_PrepareEx(vmsh, PawnHandleAddress, sizeof(CHandle), reinterpret_cast<BYTE*>(&m_hPawn), nullptr);

		uintptr_t PlayerDataAddress = m_EntityAddress + Offsets::CCitadelPlayerController::m_PlayerDataGlobal;

		uintptr_t LevelAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iLevel;
		VMMDLL_Scatter_PrepareEx(vmsh, LevelAddress, sizeof(int32_t), reinterpret_cast<BYTE*>(&m_CurrentLevel), nullptr);

		uintptr_t MaxHealthAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iHealthMax;
		VMMDLL_Scatter_PrepareEx(vmsh, MaxHealthAddress, sizeof(int32_t), reinterpret_cast<BYTE*>(&m_MaxHealth), nullptr);

		uintptr_t CurrentHealthAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iHealth;
		VMMDLL_Scatter_PrepareEx(vmsh, CurrentHealthAddress, sizeof(int32_t), reinterpret_cast<BYTE*>(&m_CurrentHealth), nullptr);

		uintptr_t HeroIDAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_nHeroID;
		VMMDLL_Scatter_PrepareEx(vmsh, HeroIDAddress, sizeof(int32_t), reinterpret_cast<BYTE*>(&m_HeroID), nullptr);

		uintptr_t TotalSoulsAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_nTotalSouls;
		VMMDLL_Scatter_PrepareEx(vmsh, TotalSoulsAddress, sizeof(int32_t), reinterpret_cast<BYTE*>(&m_TotalSouls), nullptr);

		uintptr_t UltTrainedAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_bUltimateTrained;
		VMMDLL_Scatter_PrepareEx(vmsh, UltTrainedAddress, sizeof(bool), reinterpret_cast<BYTE*>(&m_bUltimateTrained), nullptr);

		uintptr_t UltCDStartAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_flUltimateCooldownStart;
		VMMDLL_Scatter_PrepareEx(vmsh, UltCDStartAddress, sizeof(float), reinterpret_cast<BYTE*>(&m_flUltimateCooldownStart), nullptr);

		uintptr_t UltCDEndAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_flUltimateCooldownEnd;
		VMMDLL_Scatter_PrepareEx(vmsh, UltCDEndAddress, sizeof(float), reinterpret_cast<BYTE*>(&m_flUltimateCooldownEnd), nullptr);
	}

	void QuickRead(VMMDLL_SCATTER_HANDLE vmsh)
	{
		if (IsInvalid())
			return;

		CBaseEntity::QuickRead(vmsh, false);

		uintptr_t PlayerDataAddress = m_EntityAddress + Offsets::CCitadelPlayerController::m_PlayerDataGlobal;

		uintptr_t CurrentHealthAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iHealth;
		VMMDLL_Scatter_PrepareEx(vmsh, CurrentHealthAddress, sizeof(int32_t), reinterpret_cast<BYTE*>(&m_CurrentHealth), nullptr);
	}
private:

};
