#include "pch.h"

#include "HeroSkeletonMap.hpp"

// The one translation unit that pays for the generated table. See the note in
// HeroSkeletonMap.hpp before moving this include anywhere else.
#include "BoneLists.hpp"

const ModelBoneData* GetHeroBoneData(std::string_view modelPath) noexcept
{
    auto it = g_HeroModelData.find(std::string(modelPath));
    return (it != g_HeroModelData.end()) ? &it->second : nullptr;
}

int GetHeroBoneSlot(std::string_view modelPath, HitboxSlot slot) noexcept
{
    const ModelBoneData* data = GetHeroBoneData(modelPath);
    if (!data) return -1;
    const auto& bones = data->slotBones[static_cast<int>(slot)];
    return bones.empty() ? -1 : static_cast<int>(bones[0]);
}

const std::vector<BonePair>* GetHeroBonePairs(std::string_view modelPath) noexcept
{
    const ModelBoneData* data = GetHeroBoneData(modelPath);
    return data ? &data->pairs : nullptr;
}
