#pragma once
#include "BoneLists.hpp"

inline const ModelBoneData* GetHeroBoneData(std::string_view modelPath) noexcept
{
    auto it = g_HeroModelData.find(std::string(modelPath));
    return (it != g_HeroModelData.end()) ? &it->second : nullptr;
}

// Returns the first bone index for the given slot, or -1 if unavailable.
inline int GetHeroBoneSlot(std::string_view modelPath, HitboxSlot slot) noexcept
{
    const ModelBoneData* data = GetHeroBoneData(modelPath);
    if (!data) return -1;
    const auto& bones = data->slotBones[static_cast<int>(slot)];
    return bones.empty() ? -1 : static_cast<int>(bones[0]);
}

// Returns the bone pairs for skeleton drawing, or nullptr if unavailable.
inline const std::vector<BonePair>* GetHeroBonePairs(std::string_view modelPath) noexcept
{
    const ModelBoneData* data = GetHeroBoneData(modelPath);
    return data ? &data->pairs : nullptr;
}