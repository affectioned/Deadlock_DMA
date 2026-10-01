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

const std::vector<BonePair>* GetHeroBonePairs(std::string_view modelPath) noexcept
{
    const ModelBoneData* data = GetHeroBoneData(modelPath);
    return data ? &data->pairs : nullptr;
}
