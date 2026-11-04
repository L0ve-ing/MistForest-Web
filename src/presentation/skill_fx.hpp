#pragma once
#include "../game.hpp"
#include <cstdint>

namespace mist::presentation {
using Tick = std::uint64_t;
// Cosmetic descriptors contain only filtered View data and public attack cells.
constexpr Tick SkillFxDuration = 600;
enum class FxStyle {
    None,
    Cross,
    Snipe,
    Bite,
    Shotgun,
    ShadowCut,
    Charge,
    Arrow,
    Claw,
    CrimsonClaw,
    Thrust,
    Fire,
    RosePlant,
    RoseBloom,
    SwordStab,
    SwordThrow,
    SwordRecall,
    Dash,
    Transform,
    ShadowLink,
    Blood,
    ArmorUp,
    ArmorDown,
    Silence,
    Jump,
    Build,
    Demolish,
    FireStep,
    Advance,
    Burn
};

struct SkillFx {
    FxStyle style = FxStyle::None;
    Role role = Role::Sniper;
    Kind kind = Kind::End;
    int form = 1;
    int anchor = 0, linkSubject = -1;
    Mask area{}, links{};
    Mask cells{}, attacks{}, buds{}, petals{}, wither{}, health{}, shield{}, impact{};
};

bool fxAny(const Mask &cells);
bool fxVisible(const SkillFx &effect);
FxStyle skillStyle(Role role, Kind kind, int form);
// Pass the same viewer's notices explicitly: compact history frames omit them.
SkillFx makeSkillFx(const View &before, const View &after, const Notice &notice,
                    const Mask &attacks, const std::vector<Notice> &notices);
} // namespace mist::presentation
