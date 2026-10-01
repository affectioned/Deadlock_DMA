#include "pch.h"

#include "Deadlock.h"
#include "GameModules.h"
#include "Entity List/EntityList.h"
#include "GUI/Fuser/Fuser.h"

#include <numbers>

bool Deadlock::Initialize(DMA_Connection* Conn)
{
	auto& Process = Deadlock::Proc();

	Process.GetProcessInfo(GameModules::ProcessName, GameModules::ModuleList(), Conn);

	Offsets::ResolveOffsets(Conn);

	EntityList::InitScatterHandle(Conn, &Process);
	EntityList::FullUpdate(Conn, &Process);

	uintptr_t clientBase = Process.GetModuleBase(GameModules::ClientDll);
	m_PredictionAddress = clientBase ? clientBase + Offsets::Prediction : 0;
	Log::Info("Prediction: 0x{:X}", m_PredictionAddress);

	UpdateLocalPlayerAddresses(Conn);

	Log::Info("[Init] ready");

	return true;
}

Process& Deadlock::Proc()
{
	return m_DeadlockProc;
}

void Deadlock::UpdateViewMatrix(DMA_Connection* Conn)
{
	uintptr_t ViewMatrixAddress = Proc().GetModuleBase(GameModules::ClientDll) + Offsets::ViewMatrix;
	Matrix44 NewMatrix = Proc().ReadMem<Matrix44>(Conn, ViewMatrixAddress);

	// Yaw is purely a function of the view matrix's first row — derive it here
	// instead of scheduling a separate timer, which used to round-trip through
	// GetViewMatrix()/SetClientYaw() (~1080 redundant ticks per 10 s).
	float NewYaw = atan2(NewMatrix.m01, NewMatrix.m00);

	ApplyViewMatrix(NewMatrix);
}

void Deadlock::ApplyViewMatrix(const Matrix44& mat)
{
	float yaw = atan2(mat.m01, mat.m00);
	{
		std::scoped_lock lock(ViewMatrixMutex);
		m_ViewMatrix = mat;
	}
	SetClientYaw(yaw);
}

Matrix44 Deadlock::GetViewMatrix()
{
	std::scoped_lock lock(ViewMatrixMutex);
	return m_ViewMatrix;
}

bool Deadlock::WorldToScreen(const Vector3& Pos, Vector2& ScreenPos)
{
	Matrix44 Mat;
	{
		std::scoped_lock lock(ViewMatrixMutex);
		Mat = m_ViewMatrix;
	}
	return WorldToScreen(Mat, Pos, ScreenPos);
}

bool Deadlock::WorldToScreen(const Matrix44& Mat, const Vector3& Pos, Vector2& ScreenPos)
{
	ScreenPos.x = Mat.m00 * Pos.x + Mat.m01 * Pos.y + Mat.m02 * Pos.z + Mat.m03;
	ScreenPos.y = Mat.m10 * Pos.x + Mat.m11 * Pos.y + Mat.m12 * Pos.z + Mat.m13;

	float w = Mat.m30 * Pos.x + Mat.m31 * Pos.y + Mat.m32 * Pos.z + Mat.m33;

	if (w < 0.01f)
		return false;

	float inv_w = 1.f / w;
	ScreenPos.x *= inv_w;
	ScreenPos.y *= inv_w;

	float x = Fuser::m_ScreenSize.x * .5f;
	float y = Fuser::m_ScreenSize.y * .5f;

	x += 0.5f * ScreenPos.x * Fuser::m_ScreenSize.x + 0.5f;
	y -= 0.5f * ScreenPos.y * Fuser::m_ScreenSize.y + 0.5f;

	ScreenPos.x = x;
	ScreenPos.y = y;

	return true;
}

bool Deadlock::UpdateLocalPlayerAddresses(DMA_Connection* Conn)
{
	std::scoped_lock Lock(m_LocalAddressMutex);

	uintptr_t clientBase = Proc().GetModuleBase(GameModules::ClientDll);

	// Pawn first, controller derived from it. The previous order started at a
	// hand-made LocalController pattern that had no upstream source and broke
	// on the 2026-10-01 build, reading 0xC; dwLocalPlayerPawn rides the
	// CPrediction scan instead, which the dump does publish a pattern for.
	uintptr_t newPawn = Proc().ReadMem<uintptr_t>(Conn, clientBase + Offsets::LocalPlayerPawn);

	// Anything that is not a plausible heap pointer means the global moved.
	// Left unchecked it propagates as a confident wrong answer to every
	// "who am I" consumer, so drop it here and say so once.
	if (newPawn && newPawn < 0x10000)
	{
		static bool bWarnedBadPawn = false;
		if (!bWarnedBadPawn)
		{
			Log::Warn("[Local] LocalPlayerPawn read 0x{:X} — stale pattern/offset. "
			          "Team falls back to the FOW team entity; local-pawn features stay off.", newPawn);
			bWarnedBadPawn = true;
		}
		newPawn = 0;
	}

	CHandle hController{ 0 };
	if (newPawn)
		hController = Proc().ReadMem<CHandle>(Conn,
			newPawn + Offsets::C_BasePlayerPawn::m_hController);

	uintptr_t newCtrl = EntityList::GetEntityAddressFromHandle(hController);

	// A 0 read here is almost always a transient scatter failure (alt-tab).
	// Preserve the last-known-good ptrs so visuals don't disappear until the
	// next successful read.
	if (newCtrl && newCtrl != m_LocalPlayerControllerAddress)
	{
		m_LocalPlayerControllerAddress = newCtrl;
		Log::Info("LocalCtrl: 0x{:X}", m_LocalPlayerControllerAddress);
	}

	if (newPawn && newPawn != m_LocalPlayerPawnAddress)
	{
		m_LocalPlayerPawnAddress = newPawn;
		Log::Info("LocalPawn: 0x{:X}", m_LocalPlayerPawnAddress);
	}

	// Both pawn and controller no longer register a class name (pName == 0
	// in CEntityIdentity), so every remote player is invisible to the class-
	// map sort. Snapshot the vtables off the local player once and let
	// EntityList::DiscoverPlayersByVTable classify the rest by vtable match.
	uintptr_t pawnVT = newPawn ? Proc().ReadMem<uintptr_t>(Conn, newPawn) : 0;
	uintptr_t ctrlVT = newCtrl ? Proc().ReadMem<uintptr_t>(Conn, newCtrl) : 0;

	// During lobby->game transitions the entity memory is being rebuilt and
	// the read can land on uninitialised data (e.g. 0x3F80000000000000 = 1.0f).
	// Reject anything outside client.dll so garbage doesn't poison the cache.
	size_t clientSize = Proc().GetModuleSize(GameModules::ClientDll);
	auto inClient = [&](uintptr_t vt) { return vt >= clientBase && vt < clientBase + clientSize; };
	if (!inClient(pawnVT)) pawnVT = 0;
	if (!inClient(ctrlVT)) ctrlVT = 0;

	EntityList::CachePlayerVTables(pawnVT, ctrlVT);

	return true;
}

void Deadlock::UpdateServerTime(DMA_Connection* Conn)
{
	// Initialize() derives m_PredictionAddress once at startup; if the module
	// base wasn't resolved yet (game still spinning up), retry here so
	// ServerTime doesn't stay dead for the whole session.
	if (!m_PredictionAddress)
	{
		uintptr_t clientBase = Proc().GetModuleBase(GameModules::ClientDll);
		if (!clientBase) return;
		m_PredictionAddress = clientBase + Offsets::Prediction;
		Log::Info("Prediction: 0x{:X}", m_PredictionAddress);
	}

	uintptr_t ServerTimeAddress = m_PredictionAddress + Offsets::CPrediction::ServerTime;

	std::scoped_lock lock(m_ServerTimeMutex);
	m_ServerTime = Proc().ReadMem<float>(Conn, ServerTimeAddress);
	m_ServerTimeUpdatedAt = std::chrono::steady_clock::now();
}

void Deadlock::SetClientYaw(float NewYaw)
{
	std::scoped_lock Lock(m_ClientYawMutex);
	m_ClientYaw = NewYaw;
}

float Deadlock::GetClientYaw()
{
	std::scoped_lock Lock(m_ClientYawMutex);
	return m_ClientYaw;
}

float Deadlock::GetClientYawDegrees()
{
	std::scoped_lock Lock(m_ClientYawMutex);

	return m_ClientYaw * (180.0f / std::numbers::pi_v<float>);
}
