#include "pch.h"
#include "LastKnown.h"

#include "Snapshot.h"
#include "WorldText.h"
#include "Draw/Players.h"
#include "GUI/Color Picker/Color Picker.h"

#include <numbers>

namespace
{
	struct Track
	{
		Vector3          position{ 0.0f };
		double           seenAt{ 0.0 };
		ETeam            team{ ETeam::UNKNOWN };
		std::string_view heroName{};
	};

	// Keyed by controller address: the controller outlives the pawn across
	// dormancy and stays networked for every player, so it's the stable id.
	// Only touched from the GUI thread (Snapshot::Acquire / DrawWorld).
	std::unordered_map<uintptr_t, Track> s_Tracks;
	ETeam                                s_TracksTeam{ ETeam::UNKNOWN };

	double NowSeconds()
	{
		return std::chrono::duration<double>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	ImU32 TeamColor(ETeam team)
	{
		const ImColor& c = team == ETeam::HIDDEN_KING
			? ColorPicker::HiddenKingTeamColor
			: ColorPicker::ArchMotherTeamColor;
		return ImGui::ColorConvertFloat4ToU32(c.Value);
	}

	// Ring on the ground plane around a world point. Each vertex is projected on
	// its own, and a segment is drawn only when both ends project, so a ring
	// that passes behind the camera is clipped rather than smeared across the
	// screen.
	void DrawGroundRing(const FrameSnapshot& snap, ImDrawList* dl, const ImVec2& origin,
	                    const Vector3& center, float radius, ImU32 col)
	{
		constexpr int kSegments = 40;

		ImVec2 prev{};
		bool   prevOk = false;

		// i == kSegments lands back on the first vertex and closes the ring.
		for (int i = 0; i <= kSegments; ++i)
		{
			const float a = (static_cast<float>(i) / kSegments) * 2.0f * std::numbers::pi_v<float>;
			const Vector3 p{ center.x + radius * std::cos(a), center.y + radius * std::sin(a), center.z };

			ImVec2 sp;
			const bool ok = snap.Project(p, origin, sp);

			if (ok && prevOk) dl->AddLine(prev, sp, col, 1.5f);

			prev   = sp;
			prevOk = ok;
		}
	}
}

void LastKnown::Update(FrameSnapshot& snap)
{
	snap.ghosts.clear();

	// Without a known local team, "enemy" can't be decided, and the history
	// from the previous match/team is meaningless — start over.
	if (!bEnabled || !snap.haveLocalTeam)
	{
		s_Tracks.clear();
		return;
	}
	if (snap.localTeam != s_TracksTeam)
	{
		s_Tracks.clear();
		s_TracksTeam = snap.localTeam;
	}

	const double now = NowSeconds();

	// Per-frame state of every enemy controller present in the snapshot.
	// Controllers missing from here have no pawn this frame at all, which
	// counts the same as dormant.
	struct Presence { bool live; };
	std::unordered_map<uintptr_t, Presence> present;
	present.reserve(snap.players.size());

	for (const auto& view : snap.players)
	{
		if (view.friendly || view.localPlayer) continue;

		const uintptr_t key = view.controller->m_EntityAddress;

		// Dead players respawn at base — a marker where they died would be a
		// lie. Drop the track; they'll get a fresh one once seen again.
		if (view.controller->IsDead())
		{
			s_Tracks.erase(key);
			continue;
		}

		const bool live = !view.pawn->IsDormant();
		const bool seen = live && (!Draw_Players::bVisibleOnly || view.confirmedVisible);

		present[key] = Presence{ live };

		if (seen)
		{
			Track& t   = s_Tracks[key];
			t.position = view.pawn->m_Position;
			t.seenAt   = now;
			t.team     = view.controller->m_TeamNum;
			t.heroName = view.controller->GetHeroName();
		}
	}

	const float maxReach = std::max(fMaxReachM, 0.0f) * HammerUnitsPerMeter;
	const float speed    = std::max(fMoveSpeedMps, 0.0f) * HammerUnitsPerMeter;

	for (auto it = s_Tracks.begin(); it != s_Tracks.end(); )
	{
		const Track& t   = it->second;
		const float  age = static_cast<float>(now - t.seenAt);

		if (age > fMaxAgeSec)
		{
			it = s_Tracks.erase(it);
			continue;
		}

		// Seen this very frame — the live ESP is drawing it.
		if (age <= 0.0f)
		{
			++it;
			continue;
		}

		const auto p = present.find(it->first);

		FrameSnapshot::Ghost g;
		g.position   = t.position;
		g.ageSec     = age;
		g.reachUnits = std::min(age * speed, maxReach);
		g.team       = t.team;
		g.heroName   = t.heroName;
		g.stale      = p == present.end() || !p->second.live;
		snap.ghosts.push_back(g);

		++it;
	}
}

void LastKnown::DrawWorld()
{
	if (!bEnabled || !bWorld) return;

	const FrameSnapshot& snap = Snapshot::Current();
	if (snap.ghosts.empty()) return;

	const ImVec2 origin = ImGui::GetWindowPos();
	ImDrawList*  dl     = ImGui::GetWindowDrawList();

	for (const auto& g : snap.ghosts)
	{
		const float distM = snap.haveLocalPosition
			? g.position.Distance(snap.localPosition) / HammerUnitsPerMeter
			: 0.0f;

		const WorldText::Falloff falloff = WorldText::Compute(distM);
		if (falloff.alpha <= 0.0f) continue;

		// Fades from 0.9 to 0.3 over the marker's life, so a fresh sighting
		// reads as more trustworthy than an old one.
		const float life  = std::clamp(1.0f - g.ageSec / std::max(fMaxAgeSec, 0.1f), 0.0f, 1.0f);
		const float alpha = falloff.alpha * (0.3f + 0.6f * life);
		const ImU32 col   = WorldText::Fade(TeamColor(g.team), alpha);

		if (bShowReach && g.reachUnits > 0.0f)
			DrawGroundRing(snap, dl, origin, g.position, g.reachUnits, WorldText::Fade(col, 0.6f));

		ImVec2 anchor;
		if (!snap.Project(g.position, origin, anchor)) continue;

		dl->AddCircle(anchor, 5.0f, col, 0, 1.5f);

		WorldText::Stack stack(dl, ImVec2(anchor.x, anchor.y + 6.0f), falloff.size);
		stack.Push(std::format("{}?", g.heroName), col);
		stack.Push(std::format("{:.0f}s ago", g.ageSec), col);
	}
}
