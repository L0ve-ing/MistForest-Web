#include "skill_fx.hpp"
#include <algorithm>

namespace mist::presentation {

bool fxAny(const Mask &cells) {
    return std::any_of(cells.begin(), cells.end(), [](bool x) { return x; });
}

bool fxVisible(const SkillFx &effect) {
    return fxAny(effect.cells) || fxAny(effect.wither);
}

FxStyle skillStyle(Role role, Kind kind, int form) {
    switch (kind) {
    case Kind::PlantRose:
        return FxStyle::RosePlant;
    case Kind::Bloom:
        return FxStyle::RoseBloom;
    case Kind::SwordStab:
        return FxStyle::SwordStab;
    case Kind::ThrowSword:
        return FxStyle::SwordThrow;
    case Kind::RecallSword:
        return FxStyle::SwordRecall;
    case Kind::Dash:
        return FxStyle::Dash;
    case Kind::Transform:
        return FxStyle::Transform;
    case Kind::Follow:
    case Kind::Lead:
    case Kind::ShadowMode:
        return FxStyle::ShadowLink;
    case Kind::Blood:
        return FxStyle::Blood;
    case Kind::ArmorUp:
        return FxStyle::ArmorUp;
    case Kind::ArmorDown:
        return FxStyle::ArmorDown;
    case Kind::Silence:
        return FxStyle::Silence;
    case Kind::Leap:
    case Kind::Retreat:
        return FxStyle::Jump;
    case Kind::BuildFort:
    case Kind::BuildWall:
        return FxStyle::Build;
    case Kind::Demolish:
        return FxStyle::Demolish;
    case Kind::Charge:
    case Kind::Hunt:
        return FxStyle::Charge;
    case Kind::Thrust:
        return FxStyle::Thrust;
    case Kind::Flame:
        return FxStyle::Fire;
    case Kind::FireStep:
        return FxStyle::FireStep;
    case Kind::FlameAdvance:
        return FxStyle::FireStep;
    case Kind::SwordAdvance:
        return FxStyle::Advance;
    case Kind::Cleave:
    case Kind::CleaveRush:
        return FxStyle::Cross;
    case Kind::End:
        return FxStyle::Burn;
    case Kind::Attack:
    case Kind::AttackRush:
        switch (role) {
        case Role::Sniper:
            return FxStyle::Snipe;
        case Role::Wolf:
            return FxStyle::Bite;
        case Role::Ink:
            return FxStyle::Shotgun;
        case Role::Shadow:
            return FxStyle::ShadowCut;
        case Role::HiddenWolf:
            return form ? FxStyle::Claw : FxStyle::Arrow;
        case Role::Alpha:
            return FxStyle::CrimsonClaw;
        case Role::Armiger:
        case Role::WallGuard:
            return FxStyle::Thrust;
        default:
            return FxStyle::Cross;
        }
    case Kind::Move:
        return role == Role::Shadow ? FxStyle::Dash : FxStyle::None;
    default:
        return FxStyle::None;
    }
}

SkillFx makeSkillFx(const View &before, const View &after, const Notice &notice,
                    const Mask &attacks, const std::vector<Notice> &notices) {
    SkillFx out;
    out.kind = notice.kind;
    out.attacks = out.cells = attacks;
    int id = notice.unit;
    if (id < 0 || id >= after.playerCount * 2 || id % 2 >= after.teamSize)
        return out;
    const auto &actor = after.units[id];
    out.role = actor.role;
    out.form = actor.form;
    out.style = skillStyle(actor.role, notice.kind, actor.form);
    if (actor.role == Role::Heavy) {
        // The referee publishes charge cells only at the end, without their order.
        if (notice.kind == Kind::Charge && !fxAny(attacks))
            out.style = FxStyle::Dash;
        if (notice.kind == Kind::End && fxAny(attacks))
            for (const auto &n : notices)
                if (n.revision == notice.revision && n.attacks == attacks &&
                    n.effect == PublicEffect::Charge) {
                    out.style = FxStyle::Charge;
                    break;
                }
    }
    auto mark = [&](int cell, Mask &mask) {
        if (cell > 0 && cell <= cellCount(after.mode))
            mask[cell - 1] = true;
    };
    if (notice.kind == Kind::Silence) {
        // Hunt's landing is public; target positions inside the area remain private.
        int origin = notice.player == after.player ? before.budget.anchor : 0;
        if (!origin)
            for (const auto &n : notices)
                if (n.unit == id && n.turn == notice.turn && n.revision <= notice.revision &&
                    n.kind == Kind::Hunt &&
                    std::count(n.attacks.begin(), n.attacks.end(), true) == 1)
                    origin = int(std::find(n.attacks.begin(), n.attacks.end(), true) -
                                 n.attacks.begin()) +
                             1;
        if (!origin && notice.player == after.player)
            origin = actor.position;
        out.anchor = origin;
        if (origin > 0 && origin <= cellCount(after.mode))
            for (int cell = 1; cell <= cellCount(after.mode); ++cell)
                if (silenceArea(after.mode, origin, cell, after.map)) {
                    mark(cell, out.area);
                    mark(cell, out.cells);
                }
    }
    if (notice.player == after.player && notice.kind == Kind::Follow && actor.follow >= 0 &&
        actor.follow < 8) {
        out.linkSubject = actor.follow;
        mark(actor.position, out.links);
        int cell = soldierCell(after.units[actor.follow], actor.followPiece);
        mark(cell, out.links);
        mark(cell, out.cells);
    }
    // Only the owner can place cosmetics on a private movement/skill position.
    if (notice.player == after.player && out.style != FxStyle::None && !fxAny(attacks) &&
        notice.kind != Kind::End && notice.kind != Kind::BuildFort &&
        notice.kind != Kind::BuildWall && notice.kind != Kind::Demolish) {
        for (int piece = 0; piece < (actor.role == Role::WallGuard ? 3 : 1); ++piece) {
            mark(soldierCell(actor, piece), out.cells);
            if (out.style == FxStyle::Dash || out.style == FxStyle::Jump ||
                out.style == FxStyle::FireStep || out.style == FxStyle::Advance)
                mark(soldierCell(before.units[id], piece), out.cells);
        }
    }
    for (const auto &rose : after.roses) {
        if (rose.health <= 0)
            continue;
        auto old = std::find_if(before.roses.begin(), before.roses.end(), [&](const Rose &r) {
            return r.unit == rose.unit && r.id == rose.id;
        });
        if (old == before.roses.end() || old->health <= 0) {
            mark(rose.cell, out.buds);
            mark(rose.cell, out.cells);
        } else if (rose.health < old->health && notice.kind == Kind::Bloom) {
            mark(rose.cell, out.petals);
            mark(rose.cell, out.cells);
        }
    }
    for (const auto &rose : before.roses) {
        if (rose.health <= 0)
            continue;
        auto now = std::find_if(after.roses.begin(), after.roses.end(), [&](const Rose &r) {
            return r.unit == rose.unit && r.id == rose.id && r.health > 0;
        });
        if (now == after.roses.end() &&
            (!fxAny(attacks) || attacks[rose.cell - 1] || notice.kind == Kind::Bloom))
            mark(rose.cell, out.wither);
    }
    if (notice.kind == Kind::RecallSword)
        for (const auto &sword : before.swords) {
            auto now = std::find_if(after.swords.begin(), after.swords.end(), [&](const Sword &x) {
                return x.unit == sword.unit && x.cell == sword.cell;
            });
            if (now == after.swords.end())
                mark(sword.cell, out.cells);
        }
    if (notice.kind == Kind::BuildFort || notice.kind == Kind::BuildWall ||
        notice.kind == Kind::Demolish)
        for (int c = 0; c < cellCount(after.mode); ++c)
            if (before.buildings[c].kind != after.buildings[c].kind ||
                before.buildings[c].health != after.buildings[c].health)
                out.cells[c] = true;
    // No enemy-location inference: hit/shield rings are placed only on own known cells.
    for (int i = after.player * 2; i < after.player * 2 + after.teamSize; ++i) {
        const auto &a = before.units[i];
        const auto &b = after.units[i];
        for (int p = 0; p < (a.role == Role::WallGuard ? 3 : 1); ++p) {
            int cell = soldierCell(a, p);
            if (cell <= 0 || cell > cellCount(after.mode) || !attacks[cell - 1])
                continue;
            bool lost = a.role == Role::WallGuard ? b.soldierHealth[p] < a.soldierHealth[p] - 1e-9
                                                  : b.health < a.health - 1e-9;
            out.health[cell - 1] = out.health[cell - 1] || lost;
            out.shield[cell - 1] =
                out.shield[cell - 1] || b.armor < a.armor - 1e-9 || b.shield < a.shield - 1e-9;
        }
    }
    // A single-cell public hit report already identifies that cell to every observer.
    // Never distribute a multi-cell hit across guessed enemy positions.
    for (const auto &n : notices) {
        if (n.revision != notice.revision ||
            (n.effect != PublicEffect::Hit && n.effect != PublicEffect::Kill) ||
            std::count(n.attacks.begin(), n.attacks.end(), true) != 1)
            continue;
        for (int c = 0; c < cellCount(after.mode); ++c)
            if (attacks[c] && n.attacks[c]) {
                out.impact[c] = true;
                out.shield[c] = out.shield[c] || n.absorbed;
            }
    }
    return out;
}

} // namespace mist::presentation
