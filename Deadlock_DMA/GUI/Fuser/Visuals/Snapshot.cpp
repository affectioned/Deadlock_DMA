#include "pch.h"
#include "Snapshot.h"
#include "Deadlock/Deadlock.h"
#include "Deadlock/Entity List/EntityList.h"
#include "GUI/Fuser/Fuser.h"

namespace
{
	// Double-buffered so the snapshot the frame is reading is never the one being
	// rebuilt, and so the vectors keep their capacity across frames instead of
	// reallocating every Acquire().
	FrameSnapshot s_Buffers[2];
	int           s_Front = 0;

	// Advance a pawn's origin and every live bone by its velocity. Bones are
	// translated rigidly — a full pose extrapolation would need per-bone
	// velocities we don't read, and over a sub-40ms window the body's linear
	// motion dominates the animation anyway.
	void Extrapolate(C_CitadelPlayerPawn& pawn, float dt)
	{
		const Vector3 delta = pawn.m_Velocity * dt;
		if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f)
			return;

		pawn.m_Position += delta;

		const int count = std::clamp(pawn.m_BoneCount, 0, MAX_BONES);
		for (int i = 0; i < count; ++i)
			pawn.m_BonePositions[i] += delta;
	}
}

bool FrameSnapshot::WorldToScreen(const Vector3& pos, Vector2& out) const
{
	return Deadlock::WorldToScreen(viewMatrix, pos, out);
}

bool FrameSnapshot::Project(const Vector3& pos, const ImVec2& origin, ImVec2& out) const
{
	Vector2 sp{};
	if (!WorldToScreen(pos, sp))
		return false;

	out = ImVec2(sp.x + origin.x, sp.y + origin.y);
	return true;
}

bool FrameSnapshot::IsFriendly(const C_BaseEntity& e) const
{
	return haveLocalTeam && e.m_TeamNum == localTeam;
}

float FrameSnapshot::DistanceMeters(const C_BaseEntity& e) const
{
	if (!haveLocalPosition)
		return 0.0f;

	return e.m_Position.Distance(localPosition) / HammerUnitsPerMeter;
}

const FrameSnapshot& Snapshot::Current()
{
	return s_Buffers[s_Front];
}

const FrameSnapshot& Snapshot::Acquire()
{
	ZoneScopedN("Snapshot::Acquire");

	FrameSnapshot& snap = s_Buffers[s_Front ^ 1];
	const auto now = std::chrono::steady_clock::now();

	snap.viewMatrix = Deadlock::GetViewMatrix();

	{
		std::scoped_lock lock(Deadlock::m_ServerTimeMutex);
		const auto elapsed = std::chrono::duration<float>(now - Deadlock::m_ServerTimeUpdatedAt).count();
		snap.serverTime = Deadlock::m_ServerTime + elapsed;
	}

	// Pawns and controllers together: the join below needs a consistent view of
	// both, and both are rebuilt under the pair by FullPawnRefresh/FullController.
	float   extrapolateDt = 0.0f;
	int32_t localPawnIndex = -1;
	{
		std::scoped_lock lock(EntityList::m_PawnMutex, EntityList::m_ControllerMutex);

		snap.pawns       = EntityList::m_PlayerPawns;
		snap.controllers = EntityList::m_PlayerControllers;

		snap.haveLocalTeam = false;
		if (EntityList::m_LocalControllerIndex >= 0 &&
		    EntityList::m_LocalControllerIndex < static_cast<int32_t>(snap.controllers.size()))
		{
			snap.localTeam     = snap.controllers[EntityList::m_LocalControllerIndex].m_TeamNum;
			snap.haveLocalTeam = true;
		}

		// Latched under the lock: the DMA thread rewrites m_LocalPawnIndex on every
		// FullPawnRefresh, and the extrapolation pass below needs the same index
		// this position came from.
		snap.haveLocalPosition = false;
		if (EntityList::m_LocalPawnIndex >= 0 &&
		    EntityList::m_LocalPawnIndex < static_cast<int32_t>(snap.pawns.size()))
		{
			localPawnIndex         = EntityList::m_LocalPawnIndex;
			snap.localPosition     = snap.pawns[localPawnIndex].m_Position;
			snap.haveLocalPosition = true;
		}

		if (Snapshot::bExtrapolate)
		{
			const int64_t stampNs = EntityList::m_PawnsUpdatedAtNs.load(std::memory_order_acquire);
			if (stampNs != 0)
			{
				const int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
					now.time_since_epoch()).count();
				const float   ageMs = static_cast<float>(nowNs - stampNs) / 1.0e6f;
				// Negative can't happen from a monotonic clock, but a zero-init
				// stamp racing in would look enormous — clamp both ends.
				extrapolateDt = std::clamp(ageMs, 0.0f, std::max(fMaxExtrapolationMs, 0.0f)) / 1000.0f;
			}
		}
	}

	if (extrapolateDt > 0.0f)
	{
		for (auto& pawn : snap.pawns)
		{
			if (pawn.IsInvalid()) continue;
			Extrapolate(pawn, extrapolateDt);
		}

		// The local origin feeds every distance readout, so it has to move with
		// the pawns or distances oscillate by the extrapolation delta.
		if (snap.haveLocalPosition)
			snap.localPosition = snap.pawns[localPawnIndex].m_Position;
	}

	{
		std::scoped_lock lock(EntityList::m_TrooperMutex);
		snap.troopers = EntityList::m_Troopers;
	}
	{
		std::scoped_lock lock(EntityList::m_MonsterCampMutex);
		snap.camps = EntityList::m_MonsterCamps;
	}
	{
		std::scoped_lock lock(EntityList::m_SinnerMutex);
		snap.sinners = EntityList::m_Sinners;
	}
	{
		std::scoped_lock lock(EntityList::m_XpOrbMutex);
		snap.xpOrbs = EntityList::m_XpOrbs;
	}
	{
		std::scoped_lock lock(EntityList::m_PowerupMutex);
		snap.powerups = EntityList::m_Powerups;
	}

	// Join pawns to controllers and resolve the per-entity predicates. Done after
	// every vector is final so the PawnView pointers stay valid for the frame.
	snap.players.clear();
	snap.players.reserve(snap.pawns.size());

	uintptr_t localPawnAddr = 0, localCtrlAddr = 0;
	{
		std::scoped_lock lock(Deadlock::m_LocalAddressMutex);
		localPawnAddr = Deadlock::m_LocalPlayerPawnAddress;
		localCtrlAddr = Deadlock::m_LocalPlayerControllerAddress;
	}

	for (const auto& pawn : snap.pawns)
	{
		if (pawn.IsInvalid()) continue;

		const uintptr_t ctrlAddr = EntityList::GetEntityAddressFromHandle(pawn.m_hController);
		if (!ctrlAddr) continue;

		const CCitadelPlayerController* ctrl = nullptr;
		for (const auto& candidate : snap.controllers)
		{
			if (candidate.m_EntityAddress == ctrlAddr)
			{
				ctrl = &candidate;
				break;
			}
		}

		if (!ctrl || ctrl->IsInvalid()) continue;

		FrameSnapshot::PawnView view{};
		view.pawn             = &pawn;
		view.controller       = ctrl;
		view.friendly         = snap.IsFriendly(*ctrl);
		view.localPlayer      = (pawn.m_EntityAddress == localPawnAddr) ||
		                        (ctrl->m_EntityAddress == localCtrlAddr);
		view.visible          = view.friendly || EntityList::IsEntityVisible(pawn.m_EntityAddress);
		view.confirmedVisible = view.friendly || EntityList::IsEntityConfirmedVisible(pawn.m_EntityAddress);
		view.distanceMeters   = snap.DistanceMeters(pawn);
		snap.players.push_back(view);
	}

	s_Front ^= 1;
	return snap;
}
