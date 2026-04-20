#pragma once
#include "C_BaseEntity.h"
#include "Deadlock/Engine/CHandle.h"
#include "Deadlock/Const/HeroMap.hpp"
#include "Deadlock/Const/HeroEnum.hpp"

class CCitadelPlayerController : public C_BaseEntity
{
public:
	using C_BaseEntity::C_BaseEntity;

	CHandle  m_hHeroPawn{ 0 };
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
	void PrepareRead_1(ScatterRead& sr)
	{
		C_BaseEntity::PrepareRead_1(sr, false);

		if (IsInvalid())
			return;

		uintptr_t PawnHandleAddress = m_EntityAddress + Offsets::CCitadelPlayerController::m_hHeroPawn;
		sr.Add(PawnHandleAddress, &m_hHeroPawn);

		uintptr_t PlayerDataAddress = m_EntityAddress + Offsets::CCitadelPlayerController::m_PlayerDataGlobal;

		uintptr_t LevelAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iLevel;
		sr.Add(LevelAddress, &m_CurrentLevel);

		uintptr_t MaxHealthAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iHealthMax;
		sr.Add(MaxHealthAddress, &m_MaxHealth);

		uintptr_t CurrentHealthAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iHealth;
		sr.Add(CurrentHealthAddress, &m_CurrentHealth);

		uintptr_t HeroIDAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_nHeroID;
		sr.Add(HeroIDAddress, &m_HeroID);

		uintptr_t TotalSoulsAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_nTotalSouls;
		sr.Add(TotalSoulsAddress, &m_TotalSouls);

		uintptr_t UltTrainedAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_bUltimateTrained;
		sr.Add(UltTrainedAddress, &m_bUltimateTrained);

		uintptr_t UltCDStartAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_flUltimateCooldownStart;
		sr.Add(UltCDStartAddress, &m_flUltimateCooldownStart);

		uintptr_t UltCDEndAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_flUltimateCooldownEnd;
		sr.Add(UltCDEndAddress, &m_flUltimateCooldownEnd);
	}

	void QuickRead(ScatterRead& sr)
	{
		if (IsInvalid())
			return;

		C_BaseEntity::QuickRead(sr, false);

		uintptr_t PlayerDataAddress = m_EntityAddress + Offsets::CCitadelPlayerController::m_PlayerDataGlobal;

		uintptr_t CurrentHealthAddress = PlayerDataAddress + Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iHealth;
		sr.Add(CurrentHealthAddress, &m_CurrentHealth);
	}
private:

};
