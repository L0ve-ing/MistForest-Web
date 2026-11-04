#include "playback.hpp"
#include <algorithm>
#include <utility>

namespace mist::presentation {
void Playback::reset() {
    state_ = PlaybackState{};
}

bool Playback::busy() const {
    return state_.running || state_.shown < state_.lines.size();
}

std::vector<sprites::Movement> Playback::movements() const {
    const auto &p = state_;
    if (!p.running || p.actionBegin >= p.lines.size())
        return {};
    const auto &line = p.lines[p.actionBegin];
    const View *before = nullptr, *after = nullptr;
    for (const auto &frame : p.frames) {
        if (frame.revision == line.revision - 1)
            before = &frame;
        if (frame.revision == line.revision)
            after = &frame;
    }
    return before && after ? sprites::visibleMovements(*before, *after, line.kind)
                           : std::vector<sprites::Movement>{};
}

const View *Playback::displayedFrame() const {
    if (busy())
        for (const auto &frame : state_.frames)
            if (frame.revision == state_.activeRevision)
                return &frame;
    return nullptr;
}

void Playback::shiftTime(Tick elapsed) {
    auto &p = state_;
    if (!p.running)
        return;
    p.nextAt += elapsed;
    p.startedAt += elapsed;
    p.motionEndsAt += elapsed;
    p.endsAt += elapsed;
    if (p.fxLive)
        p.fxAt += elapsed;
    if (p.hurtLive)
        p.hurtAt += elapsed;
    for (std::size_t i = 0; i < p.actorHit.size(); ++i)
        if (p.actorHit[i])
            p.actorHitAt[i] += elapsed;
}

double Playback::progress(Tick now) const {
    const auto &p = state_;
    if (!p.running || p.endsAt <= p.startedAt)
        return 1;
    return now <= p.startedAt
               ? 0
               : std::min(1.0, double(now - p.startedAt) / double(p.endsAt - p.startedAt));
}

double Playback::movementProgress(Tick now) const {
    const auto &p = state_;
    if (!p.running || p.motionEndsAt <= p.startedAt)
        return 1;
    return std::clamp((double(now) - double(p.startedAt)) / double(p.motionEndsAt - p.startedAt),
                      0., 1.);
}

const View *Playback::beforeFrame() const {
    const auto &p = state_;
    if (!p.running || p.actionBegin >= p.lines.size())
        return nullptr;
    for (const auto &v : p.frames)
        if (v.revision == p.lines[p.actionBegin].revision - 1)
            return &v;
    return nullptr;
}

const View *Playback::afterFrame() const {
    const auto &p = state_;
    if (!p.running || p.actionBegin >= p.lines.size())
        return nullptr;
    for (const auto &v : p.frames)
        if (v.revision == p.lines[p.actionBegin].revision)
            return &v;
    return nullptr;
}

sprites::Pose Playback::actorPose(int unit, int piece, Tick now, bool full) const {
    const auto *before = beforeFrame(), *after = afterFrame();
    if (!before || !after || unit < 0 || unit >= 8 || piece < 0 || piece > 2)
        return {};
    const auto &p = state_;
    const auto &a = before->units[unit];
    if (soldierCell(a, piece) <= 0)
        return {};
    const auto &notice = p.lines[p.actionBegin];
    int key = unit * 3 + piece;
    bool hurt = p.actorHit[key], defeated = p.actorDefeated[key];
    bool acting = notice.unit == unit && notice.kind != Kind::End && notice.kind != Kind::Move &&
                  notice.kind != Kind::ShadowMode;
    int facing = 0, origin = soldierCell(a, piece);
    for (int cell = 1; cell <= cellCount(before->mode); ++cell)
        if (notice.attacks[cell - 1]) {
            facing = ((cell - 1) % size(before->mode) > (origin - 1) % size(before->mode)) -
                     ((cell - 1) % size(before->mode) < (origin - 1) % size(before->mode));
            break;
        }
    double feedback =
        hurt && now >= p.actorHitAt[key] ? double(now - p.actorHitAt[key]) / p.fxDuration : -1;
    return sprites::actionPose(acting, hurt, defeated, movementProgress(now), feedback, facing,
                               full);
}

void Playback::applyNotice(const Notice &line) { // 文字与攻击格使用同一条已脱敏行动记录。
    auto &p = state_;
    p.activeRevision = line.revision;
    p.active = line.attacks; // 当前攻击格与同一阶段的反馈一起推进。
    if (line.player != p.viewer) {
        if (line.turn != p.enemyTurn) {
            p.enemySeen = {};
            p.enemyTurn = line.turn;
        }
        for (int c = 0; c < 81; ++c)
            p.enemySeen[c] = p.enemySeen[c] || line.attacks[c];
    }
}

Tick Playback::actionDuration(std::size_t begin, std::size_t end) const {
    bool attack = false, important = false, feedback = false;
    for (auto i = begin; i < end; ++i) {
        const auto &n = state_.lines[i];
        attack |= fxAny(n.attacks);
        important |= n.weight == 2;
        feedback |= n.feedback;
    }
    auto kind = state_.lines[begin].kind;
    bool brief =
        kind == Kind::Move || kind == Kind::Leap || kind == Kind::Retreat || kind == Kind::End;
    return important ? 2000 : attack || (!brief && feedback) ? 1400 : 800;
}

void Playback::startAction(Tick now) {
    auto &p = state_;
    if (p.shown >= p.lines.size())
        return;
    p.actionBegin = p.shown;
    p.actionEnd = p.shown + 1;
    while (p.actionEnd < p.lines.size() &&
           p.lines[p.actionEnd].revision == p.lines[p.actionBegin].revision)
        ++p.actionEnd;
    p.phases.clear();
    PlaybackPhase phase{p.actionBegin + 1, p.lines[p.actionBegin].attacks};
    for (auto i = p.actionBegin + 1; i < p.actionEnd; ++i) {
        const auto &mask = p.lines[i].attacks;
        if (fxAny(mask) && fxAny(phase.attacks) && mask != phase.attacks) {
            p.phases.push_back(phase);
            phase = {i, mask};
        } else if (fxAny(mask))
            phase.attacks = mask;
        phase.end = i + 1;
    }
    p.phases.push_back(phase);
    p.phaseIndex = 0;
    p.actorHit.fill(false);
    p.actorDefeated.fill(false);
    p.shown = p.actionBegin + 1;
    p.active = {};
    p.fxLive = false;
    p.hurtLive = false;
    p.activeRevision = std::max(0, p.lines[p.actionBegin].revision - 1);
    const auto &line = p.lines[p.actionBegin];
    if (line.player != p.viewer && line.turn != p.enemyTurn) {
        p.enemySeen = {};
        p.enemyTurn = line.turn;
    }
    p.running = true;
    p.startedAt = now;
    p.rate = speed_ / 100.;
    auto duration = [&](Tick ms) { return Tick(std::lround(ms / p.rate)); };
    p.fxDuration = duration(SkillFxDuration);
    Tick lead = fxAny(p.phases.front().attacks) || p.actionEnd > p.actionBegin + 1 ? 400 : 250;
    if (sprites::movesActor(line.kind))
        lead = 550;
    bool attacking = std::any_of(p.phases.begin(), p.phases.end(),
                                 [](const PlaybackPhase &phase) { return fxAny(phase.attacks); });
    Tick tail = attacking ? SkillFxDuration : 400;
    p.endsAt = now + duration(std::max(actionDuration(p.actionBegin, p.actionEnd),
                                       lead + (p.phases.size() - 1) * 350 + tail));
    p.nextAt = p.motionEndsAt = now + duration(lead);
}

// Damage cues compare only the viewer's own units in already-filtered frames.
bool Playback::viewerHit(const PlaybackPhase &phase, int revision) const {
    const auto &p = state_;
    const View *before = nullptr, *after = nullptr;
    for (const auto &frame : p.frames) {
        if (frame.revision == revision - 1)
            before = &frame;
        if (frame.revision == revision)
            after = &frame;
    }
    if (!before || !after || !fxAny(phase.attacks))
        return false;
    for (int i = p.viewer * 2; i < p.viewer * 2 + after->teamSize; ++i) {
        if (i < 0 || i >= 8)
            continue;
        const auto &a = before->units[i];
        const auto &b = after->units[i];
        bool loss =
            b.health < a.health - 1e-9 || b.armor < a.armor - 1e-9 || b.shield < a.shield - 1e-9;
        if (!loss)
            continue;
        for (int k = 0; k < (a.role == Role::WallGuard ? 3 : 1); ++k) {
            if (a.role == Role::WallGuard && b.soldierHealth[k] >= a.soldierHealth[k] - 1e-9)
                continue;
            for (int cell : {soldierCell(a, k), soldierCell(b, k)})
                if (cell > 0 && cell <= 81 && phase.attacks[cell - 1])
                    return true;
        }
    }
    return false;
}

bool Playback::advance(Tick now) {
    auto &p = state_;
    if (!p.running || now < p.nextAt)
        return false;
    // 延迟窗口消息不会跳过反馈，也不会挤掉最后的阅读时间。
    Tick delay = now - p.nextAt;
    p.endsAt += delay;
    if (p.phaseIndex < p.phases.size()) {
        const auto &phase = p.phases[p.phaseIndex++];
        p.shown = phase.end;
        Notice effect = p.lines[p.actionBegin];
        effect.attacks = phase.attacks;
        applyNotice(effect);
        const View *before = nullptr, *after = nullptr;
        for (const auto &frame : p.frames) {
            if (frame.revision == effect.revision - 1)
                before = &frame;
            if (frame.revision == effect.revision)
                after = &frame;
        }
        p.effect = before && after ? makeSkillFx(*before, *after, effect, phase.attacks, p.lines)
                                   : SkillFx{};
        if (before && after)
            for (int unit = 0; unit < 8; ++unit) {
                const auto &a = before->units[unit], &b = after->units[unit];
                for (int piece = 0; piece < (a.role == Role::WallGuard ? 3 : 1); ++piece) {
                    int key = unit * 3 + piece;
                    bool healthLost = a.role == Role::WallGuard
                                          ? b.soldierHealth[piece] < a.soldierHealth[piece] - 1e-9
                                          : b.health < a.health - 1e-9;
                    bool armourLost = b.armor < a.armor - 1e-9 || b.shield < a.shield - 1e-9;
                    bool attacks = fxAny(phase.attacks);
                    // Unloading armour or an expiring temporary shield is not a hit.
                    bool lost =
                        healthLost || (attacks && armourLost &&
                                       !(effect.unit == unit && effect.kind == Kind::ArmorDown));
                    bool hit = false;
                    for (int cell : {soldierCell(a, piece), soldierCell(b, piece)})
                        if (cell > 0 && cell <= cellCount(before->mode))
                            hit |= phase.attacks[cell - 1] ||
                                   (!attacks && healthLost && p.phaseIndex == p.phases.size());
                    if (lost && hit && !p.actorHit[key]) {
                        p.actorHit[key] = true;
                        p.actorHitAt[key] = now;
                        p.actorDefeated[key] =
                            a.role == Role::WallGuard
                                ? a.soldierHealth[piece] > 0 && b.soldierHealth[piece] <= 0
                                : a.health > 0 && b.health <= 0;
                        p.endsAt = std::max(p.endsAt, now + p.fxDuration);
                    }
                }
            }
        p.fxLive = fxAny(phase.attacks) || fxVisible(p.effect);
        p.fxAt = now;
        if (effect.player != p.viewer && viewerHit(phase, effect.revision)) {
            p.hurtLive = true;
            p.hurtAt = now;
        }
        p.nextAt =
            p.phaseIndex < p.phases.size() ? now + Tick(std::lround(350 / p.rate)) : p.endsAt;
        return true;
    }
    if (p.shown < p.lines.size()) {
        startAction(now);
        return true;
    }
    p.running = false;
    p.active = {};
    p.fxLive = p.hurtLive = false;
    p.frames.clear();
    return true;
}

void Playback::sync(const View &v, const std::vector<View> &history, Tick now) {
    auto &p = state_;
    bool fresh = p.viewer != v.player || v.revision < p.revision;
    if (fresh) {
        reset();
        p.viewer = v.player;
    }
    if (v.revision == p.revision)
        return;
    int previous = p.revision;
    for (const auto &line : v.notices)
        if (line.revision > previous)
            p.lines.push_back(line);
    p.revision = v.revision;
    for (int revision = std::max(0, previous); revision <= v.revision; ++revision) {
        if (std::any_of(p.frames.begin(), p.frames.end(),
                        [&](const View &frame) { return frame.revision == revision; }))
            continue;
        View frame = v;
        auto found = std::find_if(history.begin(), history.end(), [&](const View &candidate) {
            return candidate.revision == revision && candidate.player == v.player;
        });
        if (found != history.end())
            frame = *found;
        frame.notices.clear();
        p.frames.push_back(std::move(frame));
    }
    if (fresh && !p.lines.empty()) {
        int latestTurn = p.lines.back().turn;
        while (p.shown < p.lines.size() && p.lines[p.shown].turn < latestTurn)
            applyNotice(p.lines[p.shown++]);
    } // 交接/恢复只重播最近回合，旧记录仍可回看。
    if (!p.running)
        startAction(now);
}

} // namespace mist::presentation
