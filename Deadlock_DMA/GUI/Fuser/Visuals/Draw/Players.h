#pragma once
#include "Deadlock/Classes/Classes.h"
#include "Deadlock/Engine/Vector2.h"
#include "GUI/Fuser/Visuals/Snapshot.h"

enum class EHealthBarPosition : int
{
	Top    = 0,
	Bottom = 1,
	Left   = 2,
	Right  = 3,
};

enum class EBoxStyle : int
{
	Full    = 0, // closed rectangle
	Corners = 1, // four L-brackets — art-deco framing, less screen clutter
};

class Draw_Players
{
public:
	static void operator()();

public:
	static inline bool bMasterToggle{ true };
	static inline bool bHideFriendly{ false };
	static inline bool bDrawBones{ false };
	static inline float fBonesThickness{ 3.f };
	static inline bool bDrawBox{ true };
	static inline float fBoxThickness{ 1.5f };
	static inline EBoxStyle eBoxStyle{ EBoxStyle::Corners };
	// Derive the box from the projected extents of the skeleton rather than a
	// fixed 1:2 aspect guess off the head bone. Deadlock's heroes vary far too
	// much in silhouette for one hardcoded ratio to fit them all.
	static inline bool bBoxFromBones{ true };
	static inline bool bBoxFill{ false };
	static inline bool bDrawHead{ false };
	static inline bool bDrawVelocityVector{ false };
	static inline bool bDrawUnsecuredSouls{ false };
	static inline int32_t UnsecuredSoulsMinimumThreshold{ 1 };
	static inline int32_t UnsecuredSoulsHighlightThreshold{ 400 };
	static inline bool bBoneNumbers{ false };
	static inline bool bDrawHealthBar{ true };
	static inline EHealthBarPosition eHealthBarPosition{ EHealthBarPosition::Bottom };
	// Lerp the fill green → amber → red with remaining health instead of a flat
	// color. Makes a low-health target readable without reading the number.
	static inline bool bHealthGradient{ true };
	static inline bool bHideLocalPlayer{ true };
	static inline bool bShowDistance{ false };
	static inline bool bVisibleOnly{ false };
	static inline bool bShowHeroLevel{ true };
	static inline bool bShowRespawnTimer{ false };

private:
	// Everything one entity's draw pass needs, resolved once by operator().
	// Replaces threading DrawList/WindowPos/visibility through every helper.
	struct Ctx
	{
		const FrameSnapshot*            snap;
		const FrameSnapshot::PawnView*  view;
		ImDrawList*                     dl;
		ImVec2                          origin;
		// Screen-space pawn origin (feet).
		ImVec2                          feet;
		// Distance falloff for this entity: text height and a 0..1 alpha applied
		// to every element so a far target dims as a whole.
		float                           textSize;
		float                           alpha;

		const C_CitadelPlayerPawn&      Pawn() const { return *view->pawn; }
		const CCitadelPlayerController& PC()   const { return *view->controller; }
	};

	// Screen-space bounding box for a pawn. valid=false when it can't be built.
	struct Box
	{
		float left, top, right, bottom;
		bool  valid;
	};

	static Box  ComputeBox(const Ctx& c);
	static void DrawPlayer(const Ctx& c);
	static void DrawHealthBar(const Ctx& c, const Box& box, float textBottomY);
	static void DrawSkeleton(const Ctx& c);
	static void DrawBox(const Ctx& c, const Box& box);
	static void DrawHeadCircle(const Ctx& c);
	static void DrawVelocityVector(const Ctx& c);
	static void DrawBoneNumbers(const Ctx& c);
	// Returns the y coordinate just past the last text line, so the health bar's
	// Bottom mode can sit under the block without counting lines itself.
	static float DrawNameTag(const Ctx& c);
	static void DrawRespawnTimer(const Ctx& c);
};
