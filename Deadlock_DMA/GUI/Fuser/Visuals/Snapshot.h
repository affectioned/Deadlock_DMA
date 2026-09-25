#pragma once

#include "Deadlock/Classes/Classes.h"
#include "Deadlock/Engine/Matrix.h"
#include "Deadlock/Engine/Vector2.h"
#include "Deadlock/Const/ETeam.h"

// Per-frame copy of everything the overlay draws.
//
// The draw code used to hold EntityList's mutexes for the whole of its render —
// m_PawnMutex and m_ControllerMutex across every player, every trooper, every
// orb — which stalls the DMA thread's next scatter for the entire duration.
// It also called Deadlock::WorldToScreen once per point, and that takes the
// view-matrix lock on every single call: thousands of lock/unlock pairs a frame.
//
// Acquire() takes each mutex just long enough to memcpy the vectors out (tens of
// microseconds for ~40KB), then the frame renders entirely from its own copy with
// no locks at all. That also makes extrapolation possible: the snapshot owns its
// positions, so it can advance them to "now" without touching shared state.
//
// The C_BaseEntity helpers (IsFriendly / DistanceFromLocalPlayer / IsLocalPlayer)
// must NOT be called on snapshot entities — they reach back into the live
// EntityList vectors without holding a lock. Use the members precomputed here.
struct FrameSnapshot
{
	std::vector<C_CitadelPlayerPawn>      pawns;
	std::vector<CCitadelPlayerController> controllers;
	std::vector<C_NPC_Trooper>            troopers;
	std::vector<C_BaseEntity>             camps;
	std::vector<C_BaseEntity>             sinners;
	std::vector<C_BaseEntity>             xpOrbs;
	std::vector<C_BaseEntity>             powerups;

	// Pawn joined to its controller, with the per-entity predicates the draw
	// code needs already resolved. Replaces the linear std::find over
	// m_PlayerControllers that ran once per pawn, and the per-call FOW lock.
	struct PawnView
	{
		const C_CitadelPlayerPawn*      pawn{ nullptr };
		const CCitadelPlayerController* controller{ nullptr };
		bool  friendly{ false };
		bool  localPlayer{ false };
		// visible: fail-open on unknown (coloring). confirmedVisible: fail-closed
		// on unknown (hard filtering). Mirrors EntityList's two accessors.
		bool  visible{ true };
		bool  confirmedVisible{ false };
		float distanceMeters{ 0.0f };
	};
	std::vector<PawnView> players;

	Matrix44 viewMatrix{};
	ETeam    localTeam{ ETeam::UNKNOWN };
	bool     haveLocalTeam{ false };
	Vector3  localPosition{};
	bool     haveLocalPosition{ false };
	// Server time already advanced by wall-clock elapsed since the last read,
	// the same correction DrawRespawnTimer used to do inline.
	float    serverTime{ 0.0f };

	// Lock-free world→screen against the frame's cached matrix.
	bool WorldToScreen(const Vector3& pos, Vector2& out) const;

	// WorldToScreen, then offset into the host overlay window's coordinate space
	// so the result can go straight into an ImDrawList call.
	bool Project(const Vector3& pos, const ImVec2& origin, ImVec2& out) const;

	// Snapshot-safe replacements for the C_BaseEntity helpers.
	bool  IsFriendly(const C_BaseEntity& e) const;
	float DistanceMeters(const C_BaseEntity& e) const;
};

namespace Snapshot
{
	// Called once per frame at the top of Fuser::Render, before any draw pass.
	const FrameSnapshot& Acquire();

	// The snapshot Acquire() last published. Valid for the rest of the frame.
	const FrameSnapshot& Current();

	// Extrapolation. DMA polls pawns at 125 Hz; the overlay renders faster and,
	// more importantly, at an unrelated phase — so a tag is on average half a
	// poll behind, which reads as lag and jitter on a moving target. Advancing
	// position by velocity × (now − last poll) removes almost all of it.
	inline bool  bExtrapolate{ true };
	// Cap on the extrapolation window. Anything beyond this is a stalled DMA
	// read, not latency, and extrapolating it flings tags across the screen.
	inline float fMaxExtrapolationMs{ 40.0f };
}
