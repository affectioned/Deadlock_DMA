#pragma once
#include "BoneListTypes.hpp"

// BoneLists.hpp is deliberately NOT included here. The generated
// g_HeroModelData table is ~4800 lines of nested braced-init-lists of
// std::string / std::vector / std::unordered_map, which costs ~7.3s of
// compile time per translation unit that sees it (5.4s parse + 1.9s of
// emitted dynamic-initializer codegen). It used to reach 25 TUs through
// EntityList.h, i.e. ~180s of a ~320s build. Only HeroSkeletonMap.cpp
// includes it now; everything else goes through these declarations and the
// cheap type definitions in BoneListTypes.hpp.

const ModelBoneData* GetHeroBoneData(std::string_view modelPath) noexcept;

// Returns the bone pairs for skeleton drawing, or nullptr if unavailable.
const std::vector<BonePair>* GetHeroBonePairs(std::string_view modelPath) noexcept;
