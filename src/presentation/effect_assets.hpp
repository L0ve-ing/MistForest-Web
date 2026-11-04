#pragma once
#include "skill_fx.hpp"
#include <array>

namespace mist::presentation {
enum class EffectAsset {
    None = -1,
    Cross,
    SwordStab,
    RosePlant,
    ShadowCut,
    CrimsonClaw,
    Claw,
    Bite,
    Snipe,
    Shotgun,
    Fire,
    HeavyCharge,
    Arrow,
    ArmigerThrust,
    GuardThrust,
    KnightCharge,
    Silence,
    Blood,
    SwordThrow,
    SwordRecall,
    SwordLanding,
    RoseBloom,
    RoseWither,
    RosePetals,
    ArmorUp,
    ArmorDown,
    InkDash,
    ShadowDash,
    ShadowLink,
    WolfForm,
    HumanForm,
    Leap,
    Retreat,
    HeavyDash,
    FireStep,
    Burn,
    Build,
    Demolish,
    ShieldHit,
    HealthHit,
    PublicHit,
    Lake,
    Whirlpool,
    Embers
};

struct EffectSlot {
    int group, row;
};

inline constexpr std::array<EffectSlot, 43> EffectSlots{{
    {0, 0}, // Cross
    {0, 1}, // SwordStab
    {0, 2}, // RosePlant
    {1, 0}, // ShadowCut
    {1, 1}, // CrimsonClaw
    {1, 2}, // Claw
    {1, 3}, // Bite
    {2, 0}, // Snipe
    {2, 1}, // Shotgun
    {2, 2}, // Fire
    {2, 3}, // HeavyCharge
    {3, 0}, // Arrow
    {3, 1}, // ArmigerThrust
    {3, 2}, // GuardThrust
    {3, 3}, // KnightCharge
    {3, 4}, // Silence
    {3, 5}, // Blood
    {3, 6}, // SwordThrow
    {3, 7}, // SwordRecall
    {4, 0}, // SwordLanding
    {4, 1}, // RoseBloom
    {4, 2}, // RoseWither
    {4, 3}, // RosePetals
    {4, 4}, // ArmorUp
    {4, 5}, // ArmorDown
    {4, 6}, // InkDash
    {4, 7}, // ShadowDash
    {5, 0}, // ShadowLink
    {5, 1}, // WolfForm
    {5, 2}, // HumanForm
    {5, 3}, // Leap
    {5, 4}, // Retreat
    {5, 5}, // HeavyDash
    {5, 6}, // FireStep
    {5, 7}, // Burn
    {6, 0}, // Build
    {6, 1}, // Demolish
    {6, 2}, // ShieldHit
    {6, 3}, // HealthHit
    {6, 4}, // PublicHit
    {6, 5}, // Lake
    {6, 6}, // Whirlpool
    {6, 7}, // Embers
}};

constexpr EffectSlot effectSlot(EffectAsset asset) {
    int index = static_cast<int>(asset);
    return index >= 0 && index < int(EffectSlots.size()) ? EffectSlots[index] : EffectSlot{-1, -1};
}

inline constexpr std::array<EffectAsset, 8> RoleEffectAssets{
    EffectAsset::ShadowCut, EffectAsset::CrimsonClaw, EffectAsset::Claw, EffectAsset::Bite,
    EffectAsset::Snipe,     EffectAsset::Shotgun,     EffectAsset::Fire, EffectAsset::HeavyCharge};
}
