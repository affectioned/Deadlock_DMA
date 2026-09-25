#include "pch.h"

#include "Player List.h"

#include "GUI/Fuser/Visuals/Snapshot.h"

void PlayerList::Render()
{
	// Snapshot instead of locking m_PawnMutex + m_ControllerMutex for the whole
	// table build, and the pawn↔controller join is already done for us.
	const FrameSnapshot& snap = Snapshot::Current();

	if (ImGui::BeginTable("Players Table", 7))
	{
		ImGui::TableSetupColumn("Health");
		ImGui::TableSetupColumn("Hero ID");
		ImGui::TableSetupColumn("Hero Name");
		ImGui::TableSetupColumn("Distance");
		ImGui::TableSetupColumn("Souls");
		ImGui::TableSetupColumn("Pawn Address");
		ImGui::TableSetupColumn("Team ID");
		ImGui::TableHeadersRow();

		uint32_t PlayerNum = 0;

		for (const auto& view : snap.players)
		{
			const auto& Pawn = *view.pawn;
			const auto& PC   = *view.controller;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d / %d", PC.m_CurrentHealth, PC.m_MaxHealth);
			ImGui::TableNextColumn();
			ImGui::Text("%d", static_cast<int>(PC.m_HeroID));
			ImGui::TableNextColumn();
			// TextUnformatted: hero names are data, and ImGui::Text would treat a
			// '%' in one as a conversion specifier.
			ImGui::TextUnformatted(PC.GetHeroName().data());
			ImGui::TableNextColumn();
			ImGui::Text("%.2f m", view.distanceMeters);
			ImGui::TableNextColumn();
			ImGui::Text("%d", PC.m_TotalSouls);
			ImGui::TableNextColumn();
			if (ImGui::Button(std::format("Copy Address##{}", PlayerNum).c_str()))
				ImGui::SetClipboardText(std::format("0x{:X}", Pawn.m_EntityAddress).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%d", Pawn.m_TeamNum);
			PlayerNum++;
		}

		ImGui::EndTable();
	}
}
