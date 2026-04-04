#pragma once
#include "BoneLists.hpp"
#include "HeroEnum.hpp"

// Runtime map: HeroId -> model path key in g_HeroModelData.
// Empty string = unassigned (GetHeroModelPath returns nullptr).
// Assignments can be changed live via the "Hero Model Map" debug window.
inline std::unordered_map<HeroId, std::string> g_HeroModelAssignments =
{
    { HeroId::Abrams,    "models/heroes_wip/abrams/abrams.vmdl"                          },
    { HeroId::Bebop,     "models/heroes_staging/bebop/bebop.vmdl"                        },
    { HeroId::Billy,     ""                                                               },
    { HeroId::Calico,    "models/heroes_staging/nano/nano_v2/nano.vmdl"                  },
    { HeroId::Doorman,   "models/heroes_wip/doorman_v2/doorman.vmdl"                     },
    { HeroId::Drifter,   "models/heroes_wip/drifter/drifter.vmdl"                        },
    { HeroId::Dynamo,    "models/heroes_wip/dynamo/dynamo.vmdl"                          },
    { HeroId::Graves,    ""                                                               },
    { HeroId::Dummy,     ""                                                               },
    { HeroId::GreyTalon, "models/heroes_staging/archer_v2/archer_v2.vmdl"                },
    { HeroId::Haze,      "models/heroes_staging/haze/haze.vmdl"                          },
    { HeroId::Holliday,  "models/heroes_staging/astro/astro.vmdl"                        },
    { HeroId::Infernus,  "models/heroes_staging/inferno_v4/inferno.vmdl"                 },
    { HeroId::Ivy,       "models/heroes_wip/ivy/ivy.vmdl"                                },
    { HeroId::Kelvin,    "models/heroes_staging/kelvin_v2/kelvin.vmdl"                   },
    { HeroId::LadyGeist, "models/heroes_wip/geist/geist.vmdl"                            },
    { HeroId::Lash,      "models/heroes_wip/lash/lash.vmdl"                              },
    { HeroId::McGinnis,  "models/heroes_wip/mcginnis/mcginnis.vmdl"                      },
    { HeroId::Mina,      "models/heroes_wip/necro/necro.vmdl"                            },
    { HeroId::Mirage,    "models/heroes_staging/mirage_v2/mirage.vmdl"                   },
    { HeroId::MoKrill,   "models/heroes_staging/digger/digger.vmdl"                      },
    { HeroId::Paige,     "models/heroes_wip/bookworm/bookworm.vmdl"                      },
    { HeroId::Paradox,   "models/heroes_staging/chrono/chrono.vmdl"                      },
    { HeroId::Pocket,    "models/heroes_wip/pocket/pocket.vmdl"                          },
    { HeroId::Rem,       ""                                                               },
    { HeroId::Apollo,    ""                                                               },
    { HeroId::Venator,   "models/heroes_wip/fencer/fencer.vmdl"                          },
    { HeroId::Silver,    "models/heroes_wip/werewolf/werewolf.vmdl"                      },
    { HeroId::Celeste,   "models/heroes_wip/unicorn/unicorn.vmdl"                        },
    { HeroId::Seven,     "models/heroes_staging/gigawatt_prisoner/gigawatt_prisoner.vmdl"},
    { HeroId::Shiv,      "models/heroes_staging/shiv/shiv.vmdl"                          },
    { HeroId::Sinclair,  "models/heroes_staging/magician_v2/magician.vmdl"               },
    { HeroId::Victor,    "models/heroes_staging/synth/synth.vmdl"                        },
    { HeroId::Vindicta,  "models/heroes_staging/operative/operative.vmdl"                },
    { HeroId::Viscous,   "models/heroes_staging/viscous/viscous.vmdl"                    },
    { HeroId::Vyper,     "models/heroes_staging/viper/viper.vmdl"                        },
    { HeroId::Warden,    "models/heroes_staging/warden/warden.vmdl"                      },
    { HeroId::Wraith,    "models/heroes_wip/wraith/wraith.vmdl"                          },
    { HeroId::Yamato,    "models/heroes_staging/yamato_v2/yamato.vmdl"                   },
};

inline const char* GetHeroModelPath(HeroId id) noexcept
{
    auto it = g_HeroModelAssignments.find(id);
    if (it == g_HeroModelAssignments.end() || it->second.empty())
        return nullptr;
    return it->second.c_str();
}

inline const ModelBoneData* GetHeroBoneData(HeroId id) noexcept
{
    const char* path = GetHeroModelPath(id);
    if (!path) return nullptr;
    auto it = g_HeroModelData.find(path);
    return (it != g_HeroModelData.end()) ? &it->second : nullptr;
}

// Returns the first bone index for the given slot, or -1 if unavailable.
inline int GetHeroBoneSlot(HeroId id, HitboxSlot slot) noexcept
{
    const ModelBoneData* data = GetHeroBoneData(id);
    if (!data) return -1;
    const auto& bones = data->slotBones[static_cast<int>(slot)];
    return bones.empty() ? -1 : static_cast<int>(bones[0]);
}

// Returns the bone pairs for skeleton drawing, or nullptr if unavailable.
inline const std::vector<BonePair>* GetHeroBonePairs(HeroId id) noexcept
{
    const ModelBoneData* data = GetHeroBoneData(id);
    return data ? &data->pairs : nullptr;
}
