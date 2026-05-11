#pragma once

// Detects when a nearby enemy is winding up a heavy melee swing and paints a
// large on-screen "PARRY!" warning. Detection-only — does not press F. Signal:
// CCitadel_Modifier_MeleeCharge on the attacker, identified by vtable pointer
// (Offsets::MeleeChargeVTable). See CLAUDE.md for the modifier-list traversal.
class ParryWarn
{
public:
	// DMA thread: scan nearby enemy pawns' modifier lists and update warning state.
	static void RefreshState(DMA_Connection* Conn, Process* Proc);

	// GUI thread: render the warning overlay. Called from Fuser before the menu.
	static void Render();

	// Settings — persisted by Config.
	static inline bool  bMasterToggle   { true };
	// Heavy-melee reach is ~5 Hammer units (~260 hu); 400 hu gives detection lead.
	static inline float fMaxRangeHu     { 400.0f };

private:
	// Cached state populated by RefreshState, read by Render. Atomic so we don't
	// need a mutex for these — single bool + single pointer.
	static inline std::atomic<bool>      s_bWarningActive { false };
	static inline std::atomic<uintptr_t> s_CulpritAddr    { 0 };
};
