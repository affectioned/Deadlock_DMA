#pragma once

struct FrameSnapshot;

// Last-known positions for enemies we've lost sight of.
//
// When an enemy goes dormant the server stops sending its pawn, and the copy in
// client memory freezes where it was — the ESP used to keep drawing that frozen
// copy as though it were live. This remembers where each enemy was last
// actually observed and draws a fading marker there instead, with a ring that
// grows at walking speed to show how far they could have got since.
//
// "Observed" means non-dormant, and also confirmed visible on the team minimap
// when Players: Visible Only is on, so the ghost takes over exactly where the
// live ESP stops drawing.
namespace LastKnown
{
	// Rebuilds snap.ghosts from snap.players and the history kept across frames.
	// Called once per frame from Snapshot::Acquire, after players is final.
	void Update(FrameSnapshot& snap);

	// World-space markers on the Fuser canvas. The radar draws its own.
	void DrawWorld();

	inline bool  bEnabled{ true };
	inline bool  bWorld{ true };
	inline bool  bRadar{ true };
	inline bool  bShowReach{ true };
	// Forget a marker this long after the enemy was last seen. Past ~20s the
	// reach ring covers half a lane and the marker says nothing useful.
	inline float fMaxAgeSec{ 20.0f };
	// Assumed travel speed for the reach ring. Roughly base run speed; sprint
	// and movement items go faster, so treat the ring as a lower bound.
	inline float fMoveSpeedMps{ 7.0f };
	inline float fMaxReachM{ 60.0f };
}
