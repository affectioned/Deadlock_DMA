#include "pch.h"

#include "HeroSkeletonMap.hpp"

// For MAX_BONES — the bound on m_BonePositions that slot lookups must respect.
#include "Deadlock/Classes/C_CitadelPlayerPawn.h"

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

    // A slot can name a bone past MAX_BONES — some models hang hitboxes off
    // accessory bones with very high indices, and only the first MAX_BONES are
    // ever read into m_BonePositions. Take the best in-range bone instead of
    // indexing off the end of the array.
    for (int16_t bone : data->slotBones[static_cast<int>(slot)])
        if (bone >= 0 && bone < MAX_BONES)
            return static_cast<int>(bone);

    return -1;
}

const std::vector<BonePair>* GetHeroBonePairs(std::string_view modelPath) noexcept
{
    const ModelBoneData* data = GetHeroBoneData(modelPath);
    return data ? &data->pairs : nullptr;
}
