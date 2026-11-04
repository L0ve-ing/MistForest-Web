#include "action_selection.hpp"
#include <algorithm>
#include <cmath>

namespace mist::selection {
void ActionSelection::clearTargets() {
    state_.selected = state_.extra = state_.bloomLanding = 0;
}

void ActionSelection::clearPlan() {
    state_.planned.clear();
    clearTargets();
}

void ActionSelection::pointAt(int cell) {
    state_.selected = std::clamp(cell, 0, 81);
}

void ActionSelection::reset(const View &v, int preferredUnit) {
    auto &s = state_;
    int previous = s.unit;
    if (preferredUnit >= 0)
        s.unit = preferredUnit;
    if (v.budget.active >= 0)
        s.unit = v.budget.active;
    if (s.unit < v.player * 2 || s.unit >= v.player * 2 + v.teamSize ||
        v.units[s.unit].health <= 0) {
        s.unit = v.player * 2;
        if (v.units[s.unit].health <= 0 && v.teamSize > 1)
            ++s.unit;
    }
    if (s.unit != previous) {
        clearPlan();
        s.rush = s.ignite = s.lunge = false;
        s.enemyPiece = 0;
    }
    clearTargets();
    s.swordIncludesSelf = false;
    s.amount = 1;
    s.piece = 0;
    s.subject = ((v.player + 1) % v.playerCount) * 2;
    const auto &u = v.units[s.unit];
    Kind kind = u.role == Role::Prince ? Kind::PlantRose
                : u.role == Role::Sword ? Kind::SwordStab
                : u.role == Role::Knight ? Kind::Leap
                : u.role == Role::Heavy ? (v.budget.phase == 2 ? Kind::Charge : Kind::Move)
                : u.role == Role::HiddenWolf && v.budget.instinct ? Kind::Move
                : u.role == Role::Armiger ? Kind::Thrust
                : u.role == Role::Flamer ? Kind::Flame : Kind::Attack;
    if (u.role == Role::WallGuard)
        for (int k = 0; k < 3; ++k)
            if (u.soldierHealth[k] > 0 && !(v.budget.squadDone & (1 << k))) {
                s.piece = k;
                break;
            }
    if (v.budget.swordAdvance)
        kind = Kind::SwordAdvance;
    else if (v.budget.flameChoice)
        kind = Kind::FlameAdvance;
    selectKind(v, kind);
}

bool ActionSelection::selectUnit(const View &v, int unit) {
    if (unit < v.player * 2 || unit >= v.player * 2 + v.teamSize ||
        v.units[unit].health <= 0 || v.budget.active >= 0 || unit == state_.unit)
        return false;
    clearPlan();
    reset(v, unit);
    return true;
}

void ActionSelection::selectKind(const View &v, Kind kind) {
    auto &s = state_;
    const auto &u = v.units[s.unit];
    if (kind == Kind::Attack && u.role == Role::Alpha && s.rush)
        kind = Kind::AttackRush;
    if (kind == Kind::Move && u.role == Role::Shadow && v.budget.shadowMode == 1)
        kind = Kind::Lead;
    if (kind == Kind::Cleave && s.lunge)
        kind = Kind::CleaveRush;
    s.kind = kind;
    clearTargets();
    s.amount = kind == Kind::ArmorDown ? amountForSlot(v, 0) : 1;
    if (kind != Kind::SwordStab)
        s.swordIncludesSelf = false;
    // Already queued actions retain their own parameters, allowing move/attack batches.
}

bool ActionSelection::selectCell(const View &v, int cell) {
    auto &s = state_;
    if (cell < 1 || cell > cellCount(v.mode))
        return false;
    if (s.kind == Kind::BuildWall && v.units[s.unit].role != Role::WallGuard) {
        if (!s.selected || s.extra) {
            s.selected = cell;
            s.extra = 0;
        } else
            s.extra = cell;
    } else if (s.kind == Kind::Bloom && !s.extra) {
        s.extra = cell;
        s.bloomLanding = s.selected = 0;
    } else if (s.kind == Kind::Bloom && !s.bloomLanding) {
        s.bloomLanding = cell;
        s.selected = 0;
    } else if (s.batching && (s.kind == Kind::Move || s.kind == Kind::Attack ||
                             s.kind == Kind::Charge || s.kind == Kind::SwordStab)) {
        Action a = action(v, cell);
        if (s.planned.size() >= 16 || !validate(v, a))
            return false;
        s.planned.push_back(a);
        s.selected = cell;
    } else
        s.selected = cell;
    return true;
}

void ActionSelection::selectSubject(int unit) {
    if (unit < 0 || unit >= 8)
        return;
    state_.subject = unit;
    state_.enemyPiece = 0;
    clearTargets();
}

void ActionSelection::selectPiece(int piece) {
    if (piece < 0 || piece >= 3)
        return;
    state_.piece = piece;
    clearTargets();
}

void ActionSelection::cycleSubjectPiece() {
    state_.enemyPiece = (state_.enemyPiece + 1) % 3;
}

void ActionSelection::selectAmount(int amount) {
    state_.amount = std::clamp(amount, 0, 9);
}

int ActionSelection::amountForSlot(const View &v, int slot) const {
    const auto &u = v.units[state_.unit];
    bool halfArmor = u.role == Role::Armiger &&
        std::abs(u.armor - std::floor(u.armor + 1e-9)) > 1e-9;
    return slot + (halfArmor ? 0 : 1);
}

void ActionSelection::toggle(Modifier modifier) {
    auto &s = state_;
    switch (modifier) {
    case Modifier::Rush:
        s.rush = !s.rush;
        if (s.kind == Kind::Attack || s.kind == Kind::AttackRush)
            s.kind = s.rush ? Kind::AttackRush : Kind::Attack;
        break;
    case Modifier::Lunge:
        s.lunge = !s.lunge;
        if (s.kind == Kind::Cleave || s.kind == Kind::CleaveRush)
            s.kind = s.lunge ? Kind::CleaveRush : Kind::Cleave;
        break;
    case Modifier::Ignite:
        s.ignite = !s.ignite;
        break;
    case Modifier::IncludeSelf:
        s.swordIncludesSelf = !s.swordIncludesSelf;
        break;
    }
}

void ActionSelection::setBatching(bool enabled) {
    if (state_.batching != enabled)
        clearPlan();
    state_.batching = enabled;
}

Action ActionSelection::action(const View &v, int target) const {
    const auto &s = state_;
    Action a{s.unit, s.kind, target, s.subject, s.piece, s.extra, s.amount};
    if (s.kind == Kind::Transform)
        a.target = 1 - v.units[s.unit].form;
    if (s.kind == Kind::Row && target)
        a.target = (target - 1) / size(v.mode);
    if (s.kind == Kind::Col && target)
        a.target = (target - 1) % size(v.mode);
    if (s.kind == Kind::Follow || s.kind == Kind::Lead)
        a.extra = s.enemyPiece;
    if (s.kind == Kind::FireStep)
        a.amount = s.ignite ? 1 : 0;
    if (s.kind == Kind::ThrowSword)
        a.amount = distance(v.mode, v.units[s.unit].position, target);
    if (s.kind == Kind::SwordStab)
        a.amount = s.swordIncludesSelf ? 1 : 0;
    if (s.kind == Kind::Bloom)
        a.landing = s.bloomLanding;
    return a;
}

bool ActionSelection::canKind(const View &v, Kind kind) const {
    for (const auto &a : choices(v, state_.unit))
        if (a.kind == kind && (v.units[state_.unit].role != Role::WallGuard ||
                              a.piece == state_.piece))
            return true;
    return false;
}

bool ActionSelection::parameterAvailable(const View &v, int amount) const {
    Role role = v.units[state_.unit].role;
    if ((role == Role::Armiger && state_.kind != Kind::ArmorDown) ||
        (role == Role::Flamer && state_.kind != Kind::Flame))
        return false;
    for (int c = 0; c <= cellCount(v.mode); ++c) {
        auto a = action(v, c);
        a.amount = amount;
        if (validate(v, a))
            return true;
    }
    return false;
}
} // namespace mist::selection
