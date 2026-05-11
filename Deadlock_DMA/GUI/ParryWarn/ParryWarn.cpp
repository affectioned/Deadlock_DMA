#include "pch.h"
#include "ParryWarn.h"
#include "Deadlock/Deadlock.h"
#include "Deadlock/Entity List/EntityList.h"
#include "Deadlock/Offsets.h"

namespace
{
	// Cap how many modifiers we read per pawn per refresh. Real pawns rarely have
	// more than ~20 active modifiers; 32 is a safe upper bound that keeps the
	// scatter batch small. Anything beyond this gets dropped — the worst case
	// (missed detection) just means the warning is delayed by one refresh tick.
	constexpr int kMaxModifiersPerPawn = 32;

	// Cap how many enemies we scan per refresh — only the closest few matter
	// since heavy melee reach is short. Keeps the worst-case DMA work bounded
	// regardless of how many pawns are on the map.
	constexpr int kMaxEnemiesPerPass = 4;

	// CUtlVector<T*> in Source 2 is 24 bytes:
	//   [0x00] int32 count, [0x04] pad, [0x08] T** data, [0x10] int32 max
	struct UtlVecHeader
	{
		int32_t   count;
		int32_t   _pad;
		uintptr_t data;
		int32_t   max;
		int32_t   _pad2;
	};
	static_assert(sizeof(UtlVecHeader) == 24);
}

void ParryWarn::RefreshState(DMA_Connection* Conn, Process* Proc)
{
	if (!bMasterToggle || !Offsets::MeleeChargeVTable)
	{
		s_bWarningActive.store(false, std::memory_order_relaxed);
		s_CulpritAddr.store(0, std::memory_order_relaxed);
		return;
	}

	// Snapshot local pawn position + team (lock briefly, copy out, release).
	Vector3 localPos{};
	ETeam   localTeam{};
	{
		std::scoped_lock pawnLock(EntityList::m_PawnMutex);
		if (EntityList::m_LocalPawnIndex < 0 ||
		    EntityList::m_LocalPawnIndex >= (int)EntityList::m_PlayerPawns.size())
			return;
		const auto& me = EntityList::m_PlayerPawns[EntityList::m_LocalPawnIndex];
		localPos  = me.m_Position;
		localTeam = me.m_TeamNum;
	}

	// Pick the closest few enemy pawns within range. Done with a quick pawn-mutex
	// scope so we don't hold it across DMA reads.
	struct Candidate { uintptr_t addr; float distSq; };
	std::array<Candidate, kMaxEnemiesPerPass> cands{};
	int candCount = 0;

	{
		std::scoped_lock pawnLock(EntityList::m_PawnMutex);
		const float rangeSq = fMaxRangeHu * fMaxRangeHu;

		for (const auto& p : EntityList::m_PlayerPawns)
		{
			if (!p.m_EntityAddress || p.m_TeamNum == localTeam) continue;

			const float dx = p.m_Position.x - localPos.x;
			const float dy = p.m_Position.y - localPos.y;
			const float dz = p.m_Position.z - localPos.z;
			const float d2 = dx*dx + dy*dy + dz*dz;
			if (d2 > rangeSq) continue;

			// Insertion-sort into the candidate array, keep N nearest.
			int insertAt = candCount;
			for (int i = 0; i < candCount; ++i)
			{
				if (d2 < cands[i].distSq) { insertAt = i; break; }
			}
			if (insertAt < kMaxEnemiesPerPass)
			{
				const int lastSlot = std::min(candCount, kMaxEnemiesPerPass - 1);
				for (int i = lastSlot; i > insertAt; --i)
					cands[i] = cands[i - 1];
				cands[insertAt] = { p.m_EntityAddress, d2 };
				if (candCount < kMaxEnemiesPerPass) ++candCount;
			}
		}
	}

	if (candCount == 0)
	{
		s_bWarningActive.store(false, std::memory_order_relaxed);
		s_CulpritAddr.store(0, std::memory_order_relaxed);
		return;
	}

	ScatterRead sr(Conn->GetHandle(), Proc->GetPID());

	// Stage 1: read m_pModifierProp pointer for each candidate.
	std::array<uintptr_t, kMaxEnemiesPerPass> modProps{};
	for (int i = 0; i < candCount; ++i)
		sr.Add(cands[i].addr + Offsets::C_BaseEntity::m_pModifierProp, &modProps[i]);
	sr.Execute();

	// Stage 2: read the CUtlVector header (count + data ptr) from each modifier prop.
	std::array<UtlVecHeader, kMaxEnemiesPerPass> hdrs{};
	sr.Clear();
	for (int i = 0; i < candCount; ++i)
	{
		if (!modProps[i]) continue;
		sr.AddRaw(modProps[i] + Offsets::CModifierProperty::m_vecModifiers,
		          sizeof(UtlVecHeader), &hdrs[i]);
	}
	sr.Execute();

	// Stage 3: read the modifier pointer array for each candidate. Flatten into
	// one buffer so we do a single scatter for all enemies.
	std::array<std::array<uintptr_t, kMaxModifiersPerPawn>, kMaxEnemiesPerPass> modPtrs{};
	std::array<int, kMaxEnemiesPerPass> modCounts{};
	sr.Clear();
	for (int i = 0; i < candCount; ++i)
	{
		if (!hdrs[i].data || hdrs[i].count <= 0) continue;
		const int n = std::min(hdrs[i].count, kMaxModifiersPerPawn);
		modCounts[i] = n;
		sr.AddRaw(hdrs[i].data, n * sizeof(uintptr_t), modPtrs[i].data());
	}
	sr.Execute();

	// Stage 4: read the vtable pointer (first 8 bytes) from every modifier.
	// Layout-flat into one batch; index back via (candIdx, modIdx).
	std::array<std::array<uintptr_t, kMaxModifiersPerPawn>, kMaxEnemiesPerPass> vtables{};
	sr.Clear();
	for (int i = 0; i < candCount; ++i)
	{
		for (int j = 0; j < modCounts[i]; ++j)
		{
			if (!modPtrs[i][j]) continue;
			sr.Add(modPtrs[i][j], &vtables[i][j]);
		}
	}
	sr.Execute();

	// Find any enemy whose modifier list contains the MeleeCharge vtable.
	uintptr_t culprit = 0;
	for (int i = 0; i < candCount; ++i)
	{
		for (int j = 0; j < modCounts[i]; ++j)
		{
			if (vtables[i][j] == Offsets::MeleeChargeVTable)
			{
				culprit = cands[i].addr;
				break;
			}
		}
		if (culprit) break;
	}

	if (culprit)
	{
		s_bWarningActive.store(true, std::memory_order_relaxed);
		s_CulpritAddr.store(culprit, std::memory_order_relaxed);
	}
	else
	{
		s_bWarningActive.store(false, std::memory_order_relaxed);
		s_CulpritAddr.store(0, std::memory_order_relaxed);
	}
}

void ParryWarn::Render()
{
	if (!bMasterToggle) return;
	if (!s_bWarningActive.load(std::memory_order_relaxed)) return;

	// Throbbing alpha — sine wave on real time so the warning is hard to miss.
	const float t     = static_cast<float>(::GetTickCount64()) * 0.001f;
	const float pulse = 0.5f + 0.5f * sinf(t * 8.0f);  // [0..1]
	const float alpha = 0.55f + 0.45f * pulse;         // [0.55..1.0]

	ImGui::PushFont(nullptr, 72.0f);
	const char* msg = "PARRY!";
	const ImVec2 sz   = ImGui::CalcTextSize(msg);
	const ImVec2 disp = ImGui::GetIO().DisplaySize;
	const ImVec2 pos((disp.x - sz.x) * 0.5f, disp.y * 0.30f);

	// Backing plate so the text is legible against any in-game background.
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 pad(24.0f, 12.0f);
	dl->AddRectFilled(ImVec2(pos.x - pad.x, pos.y - pad.y),
	                  ImVec2(pos.x + sz.x + pad.x, pos.y + sz.y + pad.y),
	                  ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.55f * alpha)),
	                  8.0f);

	ImGui::SetCursorPos(pos);
	ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.15f, alpha), "%s", msg);
	ImGui::PopFont();
}
