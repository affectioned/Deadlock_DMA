#pragma once
#pragma once
#include "Engine/Matrix.h"
#include "Engine/Vector4.h"
#include "Engine/Vector3.h"
#include "Engine/Vector2.h"

class Deadlock
{
public:
	static bool Initialize(DMA_Connection* Conn);
	static Process& Proc();

	// Clear every per-process cache this class owns (process + modules, local
	// pawn/controller pointers, prediction address, view matrix, server time).
	// Called by the supervisor when the attached PID dies so a reattach
	// doesn't inherit addresses from the previous game instance.
	static void Reset();

private:
	static inline Process m_DeadlockProc{};

public:
	static inline std::mutex ViewMatrixMutex{};
	static inline Matrix44 m_ViewMatrix{ 0.0f };
	static void UpdateViewMatrix(DMA_Connection* Conn);
	static void ApplyViewMatrix(const Matrix44& mat);
	static Matrix44 GetViewMatrix();
	static bool WorldToScreen(const Vector3& Pos, Vector2& ScreenPos);
	// Matrix-explicit overload. The overlay snapshots the view matrix once per
	// frame and projects through this, so a frame's thousands of projections
	// don't each take ViewMatrixMutex.
	static bool WorldToScreen(const Matrix44& Mat, const Vector3& Pos, Vector2& ScreenPos);

public:
	static inline std::mutex m_LocalAddressMutex{};
	static inline uintptr_t m_LocalPlayerControllerAddress = 0;
	static inline uintptr_t m_LocalPlayerPawnAddress = 0;
	static bool UpdateLocalPlayerAddresses(DMA_Connection* Conn);

public:
	static inline std::mutex m_ServerTimeMutex{};
	static inline uintptr_t m_PredictionAddress = 0;
	static inline float m_ServerTime = 0.0f;
	static inline std::chrono::steady_clock::time_point m_ServerTimeUpdatedAt{};
	static void UpdateServerTime(DMA_Connection* Conn);


private:
	static inline std::mutex m_ClientYawMutex{};
	static inline float m_ClientYaw = 0.0f;

public:
	static void SetClientYaw(float NewYaw);
	static float GetClientYaw();
	static float GetClientYawDegrees();
};