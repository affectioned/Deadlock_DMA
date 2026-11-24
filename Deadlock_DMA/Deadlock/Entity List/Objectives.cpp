#include "pch.h"

#include "EntityList.h"
#include "GUI/Query.h"

#include <cmath>

// C_NetworkUtlVectorBase<T> layout — a CUtlVector with the network wrapper's
// vtable-free header:
//   +0x00 int32  m_Size
//   +0x08 T*     m_Memory.m_pMemory
namespace
{
	constexpr std::ptrdiff_t kVecCountOff   = 0x00;
	constexpr std::ptrdiff_t kVecDataPtrOff = 0x08;

	// The live map has ~20 camp timers. Anything past this is a stale pointer
	// read as a vector header, not a map with that many camps.
	constexpr int32_t kMaxCampTimers = 128;

	// Half the minimap extent the radar already assumes (Radar::MapSize), plus
	// slack. An origin outside this is garbage, not a camp.
	constexpr float kMaxWorldCoord = 16384.0f;

	bool PlausibleOrigin(const Vector3& v)
	{
		return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
		       std::fabs(v.x) < kMaxWorldCoord && std::fabs(v.y) < kMaxWorldCoord &&
		       std::fabs(v.z) < kMaxWorldCoord;
	}

	bool PlausibleTime(float t)
	{
		// Negative is the "no timer" sentinel (the client seeds -1.0f), so only
		// non-finite or absurd values mean the read landed on something else.
		return std::isfinite(t) && t > -1.0e6f && t < 1.0e6f;
	}

	// EGameState value from which GetMatchClock stops returning 0.
	constexpr int32_t kGameStateClockRunning = 6;
}

void EntityList::RefreshObjectiveTimers(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingObjectiveTimers() == false) return;

	const uintptr_t proxy = m_GameRulesProxyAddress;
	if (!proxy) return;

	auto Invalidate = [] {
		std::scoped_lock lk(m_ObjectiveMutex);
		m_ObjectiveTimers.valid = false;
		m_ObjectiveTimers.camps.clear();
	};

	uintptr_t rules = 0;
	m_sr->Clear();
	m_sr->Add(proxy + Offsets::C_CitadelGameRulesProxy::m_pGameRules, &rules);
	m_sr->Execute();

	if (!rules || rules < 0x10000)
	{
		Invalidate();
		return;
	}

	namespace GR = Offsets::C_CitadelGameRules;

	float    nextMidBoss = 0.0f;
	float    gameStart = 0.0f;
	int32_t  gameState = 0;
	int32_t  nOrigins = 0, nTimes = 0, nIsMid = 0;
	uint64_t pOrigins = 0, pTimes = 0, pIsMid = 0;

	m_sr->Clear();
	m_sr->Add(rules + GR::m_flGameStartTime,       &gameStart);
	m_sr->Add(rules + GR::m_eGameState,            &gameState);
	m_sr->Add(rules + GR::m_tNextMidBossSpawnTime, &nextMidBoss);
	m_sr->Add(rules + GR::m_vecNeutralCampTimerOrigins   + kVecCountOff,   &nOrigins);
	m_sr->Add(rules + GR::m_vecNeutralCampTimerOrigins   + kVecDataPtrOff, &pOrigins);
	m_sr->Add(rules + GR::m_vecNeutralCampNextSpawnTimes + kVecCountOff,   &nTimes);
	m_sr->Add(rules + GR::m_vecNeutralCampNextSpawnTimes + kVecDataPtrOff, &pTimes);
	m_sr->Add(rules + GR::m_vecNeutralCampTimerIsMidBoss + kVecCountOff,   &nIsMid);
	m_sr->Add(rules + GR::m_vecNeutralCampTimerIsMidBoss + kVecDataPtrOff, &pIsMid);
	m_sr->Execute();

	// The vectors are parallel, so only the prefix all three cover is usable.
	// IsMidBoss is optional — a missing flag array just means "plain camp".
	const int32_t count = std::min(nOrigins, nTimes);
	if (count < 0 || count > kMaxCampTimers || nIsMid < 0 || nIsMid > kMaxCampTimers ||
	    (count > 0 && (!pOrigins || !pTimes)) ||
	    !PlausibleTime(nextMidBoss) || !PlausibleTime(gameStart) || gameStart < 0.0f)
	{
		Invalidate();
		return;
	}

	std::vector<Vector3> origins(static_cast<size_t>(count));
	std::vector<float>   times(static_cast<size_t>(count));
	const int32_t        nFlags = (pIsMid && nIsMid > 0) ? std::min(nIsMid, count) : 0;
	std::vector<uint8_t> isMid(static_cast<size_t>(std::max(nFlags, 1)), 0);

	if (count > 0)
	{
		m_sr->Clear();
		m_sr->AddRaw(static_cast<uintptr_t>(pOrigins), static_cast<DWORD>(count * sizeof(Vector3)), origins.data());
		m_sr->AddRaw(static_cast<uintptr_t>(pTimes),   static_cast<DWORD>(count * sizeof(float)),   times.data());
		if (nFlags > 0)
			m_sr->AddRaw(static_cast<uintptr_t>(pIsMid), static_cast<DWORD>(nFlags), isMid.data());
		m_sr->Execute();
	}

	std::vector<CampTimer> camps;
	camps.reserve(static_cast<size_t>(count));
	for (int32_t i = 0; i < count; i++)
	{
		if (!PlausibleOrigin(origins[i]) || !PlausibleTime(times[i])) continue;

		CampTimer t;
		t.origin        = origins[i];
		t.nextSpawnTime = times[i];
		t.isMidBoss     = i < nFlags && isMid[i] != 0;
		camps.push_back(t);
	}

	// A buffer that decoded to nothing plausible is a bad pointer, not a map
	// without camps — real maps always publish their camp origins.
	if (count > 0 && camps.empty())
	{
		Invalidate();
		return;
	}

	// Logged per rules object as well as per count, so a reattach or map change
	// re-announces itself even when the camp count happens to match.
	static uintptr_t s_LoggedRules = 0;
	static int32_t   s_LoggedCount = -1;
	if (s_LoggedRules != rules || s_LoggedCount != static_cast<int32_t>(camps.size()))
	{
		s_LoggedRules = rules;
		s_LoggedCount = static_cast<int32_t>(camps.size());
		Log::Info("[Obj] rules=0x{:X} camp timers={} (raw {}/{}/{}) start={:.1f} state={}",
			rules, camps.size(), nOrigins, nTimes, nIsMid, gameStart, gameState);
	}

	std::scoped_lock lk(m_ObjectiveMutex);
	m_ObjectiveTimers.camps                = std::move(camps);
	m_ObjectiveTimers.nextMidBossSpawnTime = nextMidBoss;
	m_ObjectiveTimers.gameStartTime        = gameStart;
	m_ObjectiveTimers.clockRunning         = gameState >= kGameStateClockRunning;
	m_ObjectiveTimers.valid                = true;
}
