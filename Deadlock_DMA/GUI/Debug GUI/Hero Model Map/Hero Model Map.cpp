#include "pch.h"
#include "Hero Model Map.h"
#include "Deadlock/Const/HeroSkeletonMap.hpp"
#include "Deadlock/Const/HeroMap.hpp"

// Returns the human-readable short name for a model path.
// "models/heroes_staging/bebop/bebop.vmdl"        -> "bebop"
// "models/heroes_wip/abrams/abrams.vmdl"           -> "wip/abrams"
// "models/heroes_staging/nano/nano_v2/nano.vmdl"   -> "nano_v2"
static std::string ModelShortName(const std::string& path)
{
    bool isWip = path.find("/heroes_wip/") != std::string::npos;
    std::string_view sv(path);
    auto last = sv.rfind('/');
    auto prev = sv.rfind('/', last - 1);
    std::string dir(sv.substr(prev + 1, last - prev - 1));
    return isWip ? "wip/" + dir : dir;
}

static bool IContains(std::string_view haystack, std::string_view needle)
{
    if (needle.empty()) return true;
    return std::search(
        haystack.begin(), haystack.end(),
        needle.begin(),   needle.end(),
        [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }
    ) != haystack.end();
}

void HeroModelMap::Render()
{
    if (!bSettings) return;

    // --- Build sorted model list once ---
    struct ModelEntry { std::string shortName; std::string fullPath; };
    static std::vector<ModelEntry> s_Models;
    if (s_Models.empty())
    {
        for (const auto& [path, _] : g_HeroModelData)
            s_Models.push_back({ ModelShortName(path), path });
        std::sort(s_Models.begin(), s_Models.end(),
            [](const ModelEntry& a, const ModelEntry& b) { return a.shortName < b.shortName; });
    }

    // --- Build sorted hero list once ---
    struct HeroEntry { HeroId id; std::string name; };
    static std::vector<HeroEntry> s_Heroes;
    if (s_Heroes.empty())
    {
        for (const auto& [id, name] : HeroNames::HeroNameMap)
            s_Heroes.push_back({ id, std::string(name) });
        std::sort(s_Heroes.begin(), s_Heroes.end(),
            [](const HeroEntry& a, const HeroEntry& b) { return a.name < b.name; });
    }

    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Hero Model Map", &bSettings);

    // --- Hero name filter ---
    static char s_HeroFilter[64] = "";
    ImGui::SetNextItemWidth(200.f);
    ImGui::InputText("Filter heroes", s_HeroFilter, sizeof(s_HeroFilter));
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu heroes, %zu models)", s_Heroes.size(), s_Models.size());

    // --- Model search filter (shared across all open combos) ---
    static char s_ModelFilter[64] = "";

    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_Borders       |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_ScrollY       |
        ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("HeroModelTable", 2, kTableFlags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Hero",  ImGuiTableColumnFlags_WidthFixed, 110.f);
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (auto& hero : s_Heroes)
        {
            // Apply hero name filter
            if (!IContains(hero.name, s_HeroFilter)) continue;

            // Find this hero's current assignment
            auto assignIt = g_HeroModelAssignments.find(hero.id);
            bool hasAssignment = (assignIt != g_HeroModelAssignments.end());
            std::string& assignment = hasAssignment
                ? assignIt->second
                : g_HeroModelAssignments.emplace(hero.id, "").first->second;

            bool unassigned = assignment.empty();

            ImGui::TableNextRow();

            // --- Hero name column ---
            ImGui::TableNextColumn();
            if (unassigned)
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "%s", hero.name.c_str());
            else
                ImGui::TextUnformatted(hero.name.c_str());

            // --- Model combo column ---
            ImGui::TableNextColumn();
            std::string previewLabel = unassigned ? "???" : ModelShortName(assignment);

            ImGui::PushID(static_cast<int>(hero.id));
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::BeginCombo("##model", previewLabel.c_str()))
            {
                // Clear and focus the search box when the popup first opens
                if (ImGui::IsWindowAppearing())
                {
                    s_ModelFilter[0] = '\0';
                    ImGui::SetKeyboardFocusHere();
                }

                ImGui::SetNextItemWidth(-1.f);
                ImGui::InputText("##modelsearch", s_ModelFilter, sizeof(s_ModelFilter));

                // Unassigned option
                if (IContains("(unassigned)", s_ModelFilter))
                {
                    if (ImGui::Selectable("(unassigned)", unassigned))
                        assignment.clear();
                    if (unassigned) ImGui::SetItemDefaultFocus();
                }

                // All model entries
                for (const auto& m : s_Models)
                {
                    if (!IContains(m.shortName, s_ModelFilter) &&
                        !IContains(m.fullPath,  s_ModelFilter))
                        continue;

                    bool selected = (!unassigned && assignment == m.fullPath);
                    if (ImGui::Selectable(m.shortName.c_str(), selected))
                        assignment = m.fullPath;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", m.fullPath.c_str());
                }

                ImGui::EndCombo();
            }
            // Tooltip on the closed combo preview showing the full path
            if (!unassigned && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", assignment.c_str());

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}
