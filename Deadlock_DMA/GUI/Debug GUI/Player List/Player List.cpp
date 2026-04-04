#include "pch.h"

#include "Player List.h"

#include "Deadlock/Entity List/EntityList.h"
#include "Deadlock/Deadlock.h"

void PlayerList::Render()
{
	if (!bSettings) return;

	ImGui::Begin("Controller List", &bSettings);

	std::scoped_lock lock(EntityList::m_PawnMutex, EntityList::m_ControllerMutex);

	if (ImGui::BeginTable("Players Table", 9))
	{
		ImGui::TableSetupColumn("Health");
		ImGui::TableSetupColumn("Hero ID");
		ImGui::TableSetupColumn("Hero Name");
		ImGui::TableSetupColumn("Model");
		ImGui::TableSetupColumn("Distance");
		ImGui::TableSetupColumn("Souls");
		ImGui::TableSetupColumn("Pawn Address");
		ImGui::TableSetupColumn("Team ID");
		ImGui::TableSetupColumn("Ult CD");
		ImGui::TableHeadersRow();

		uint32_t PlayerNum = 0;

		for (auto& Pawn : EntityList::m_PlayerPawns)
		{
			uintptr_t ControllerAddr = EntityList::GetEntityAddressFromHandle(Pawn.m_hController);

			auto ControllerIt = std::find(EntityList::m_PlayerControllers.begin(), EntityList::m_PlayerControllers.end(), ControllerAddr);

			if (ControllerIt == EntityList::m_PlayerControllers.end()) continue;

			if (ControllerIt->IsInvalid() || Pawn.IsInvalid()) continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d / %d", ControllerIt->m_CurrentHealth, ControllerIt->m_MaxHealth);
			ImGui::TableNextColumn();
			ImGui::Text("%d", static_cast<int>(ControllerIt->m_HeroID));
			ImGui::TableNextColumn();
			ImGui::Text(ControllerIt->GetHeroName().data());
			// --- Model (from DMA read) ---
			ImGui::TableNextColumn();
			{
				std::string_view live = Pawn.GetModelPath();
				if (!live.empty())
				{
					auto last = live.rfind('/');
					auto prev = live.rfind('/', last - 1);
					std::string codename(live.substr(prev + 1, last - prev - 1));
					ImGui::TextUnformatted(codename.c_str());
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%.*s", static_cast<int>(live.size()), live.data());
				}
				else
				{
					ImGui::TextDisabled("...");
				}
			}
			ImGui::TableNextColumn();
			ImGui::Text("%.2f m", Pawn.DistanceFromLocalPlayer(true));
			ImGui::TableNextColumn();
			ImGui::Text("%d", ControllerIt->m_TotalSouls);
			ImGui::TableNextColumn();
			if (ImGui::Button(std::format("Copy Address##{}", PlayerNum).c_str()))
				ImGui::SetClipboardText(std::format("0x{:X}", Pawn.m_EntityAddress).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%d", Pawn.m_TeamNum);
			ImGui::TableNextColumn();
			if (!ControllerIt->m_bUltimateTrained)
			{
				ImGui::TextDisabled("not trained");
			}
			else if (ControllerIt->m_flUltimateCooldownEnd <= 0.f)
			{
				ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "READY");
			}
			else
			{
				float serverTime = Deadlock::m_ServerTime;
				float remaining = ControllerIt->m_flUltimateCooldownEnd - serverTime;
				if (remaining <= 0.f)
					ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "READY");
				else
					ImGui::Text("%ds", (int)remaining);
			}
			PlayerNum++;
		}

		ImGui::EndTable();
	}

	ImGui::End();
}