#pragma once
#include "../game.hpp"
#include <algorithm>
#include <cmath>

namespace mist::presentation::sprites {
enum class MotionStyle { Walk, Dash, Leap, Phase };

struct Waypoint {
    double column = 0, row = 0;
};

struct Movement {
    int unit = 0, piece = 0, from = 0, to = 0;
    MotionStyle style = MotionStyle::Walk;
    std::vector<Waypoint> path;
};

struct MotionSample {
    double column = 0, row = 0, lift = 0, opacity = 1;
    int facing = 0;
};

inline bool movesActor(Kind kind) {
    switch (kind) {
    case Kind::Move:
    case Kind::Lead:
    case Kind::Dash:
    case Kind::Charge:
    case Kind::Leap:
    case Kind::Hunt:
    case Kind::Retreat:
    case Kind::AttackRush:
    case Kind::CleaveRush:
    case Kind::FireStep:
    case Kind::FlameAdvance:
    case Kind::SwordAdvance:
    case Kind::Bloom:
        return true;
    default:
        return false;
    }
}

// A copied dash consists of two independent ordinary steps. On a narrow board,
// its endpoint alone cannot distinguish two steps right from one step left.
// Use a known leader when available; otherwise animate only an unambiguous route.
inline std::vector<Waypoint> copiedDashPath(const View &before, const View &after, int id) {
    const auto &unit = before.units[id];
    int w = size(before.mode), h = rows(before.mode), from = unit.position;
    auto wrap = [](int value, int length) { return (value % length + length) % length; };
    auto trace = [&](int dx, int dy, int steps) {
        int x = (from - 1) % w, y = (from - 1) / w, cell = from;
        std::vector<Waypoint> path{{double(x), double(y)}};
        for (int step = 0; step < steps; ++step) {
            int to = wrap(y + dy, h) * w + wrap(x + dx, w) + 1;
            if (to != cell && worldMove(before, id, cell, to)) {
                x += dx;
                y += dy;
                cell = to;
                path.push_back({double(x), double(y)});
            }
        }
        if (cell != after.units[id].position || path.size() < 2)
            path.clear();
        return path;
    };
    if (unit.follow >= 0 && unit.follow < 8) {
        int a = before.units[unit.follow].position, b = after.units[unit.follow].position;
        if (a > 0 && b > 0 && a <= w * h && b <= w * h) {
            int dx = (b - 1) % w - (a - 1) % w, dy = (b - 1) / w - (a - 1) / w;
            int steps = std::abs(dx) + std::abs(dy);
            if ((dx == 0 || dy == 0) && steps > 0 && steps <= 2)
                return trace((dx > 0) - (dx < 0), (dy > 0) - (dy < 0), steps);
        }
    }
    std::vector<Waypoint> selected;
    for (auto direction : {std::pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
        for (int steps : {1, 2}) {
            auto candidate = trace(direction.first, direction.second, steps);
            if (candidate.empty())
                continue;
            if (selected.empty())
                selected = candidate;
            else if (selected.size() != candidate.size() ||
                     !std::equal(selected.begin(), selected.end(), candidate.begin(),
                                 [](Waypoint a, Waypoint b) {
                                     return a.column == b.column && a.row == b.row;
                                 }))
                return {}; // Preserve uncertainty rather than invent an opposite route.
        }
    return selected;
}

// Only committed, identically filtered views. Each normal move is one legal step;
// a batch remains several revisions, so its turns are never shortcut by interpolation.
inline std::vector<Movement> visibleMovements(const View &before, const View &after, Kind kind) {
    std::vector<Movement> result;
    if (!movesActor(kind) || before.player != after.player || before.mode != after.mode)
        return result;
    const int w = size(after.mode), h = rows(after.mode);
    for (int id = 0; id < 8; ++id) {
        const auto &a = before.units[id], &b = after.units[id];
        if (a.health <= 0 || b.health <= 0 || a.role != b.role)
            continue;
        for (int piece = 0; piece < (a.role == Role::WallGuard ? 3 : 1); ++piece) {
            if (a.role == Role::WallGuard &&
                (a.soldierHealth[piece] <= 0 || b.soldierHealth[piece] <= 0))
                continue;
            int from = soldierCell(a, piece), to = soldierCell(b, piece);
            if (from < 1 || to < 1 || from > w * h || to > w * h || from == to)
                continue;
            Movement m;
            m.unit = id;
            m.piece = piece;
            m.from = from;
            m.to = to;
            Waypoint start{double((from - 1) % w), double((from - 1) / w)};
            Waypoint finish{double((to - 1) % w), double((to - 1) / w)};
            if (a.role == Role::Shadow && kind == Kind::Dash) {
                m.path = copiedDashPath(before, after, id);
                m.style = m.path.empty() ? MotionStyle::Phase : MotionStyle::Dash;
                if (m.path.empty())
                    m.path = {start, finish};
                result.push_back(std::move(m));
                continue;
            }
            double dx = finish.column - start.column, dy = finish.row - start.row;
            bool jump = kind == Kind::Leap || kind == Kind::Hunt || kind == Kind::Retreat;
            // Copied knight moves can independently fail either of their two
            // segments; endpoint-only history cannot identify that route safely.
            bool phase = kind == Kind::Bloom || kind == Kind::FlameAdvance ||
                         (jump && a.role != Role::Knight);
            if (a.role == Role::Shadow && !phase) {
                if (std::abs(dx) > w / 2.)
                    dx += dx > 0 ? -w : w;
                if (std::abs(dy) > h / 2.)
                    dy += dy > 0 ? -h : h;
                finish = {start.column + dx, start.row + dy};
            }
            m.style = phase                                      ? MotionStyle::Phase
                      : jump                                     ? MotionStyle::Leap
                      : kind == Kind::Move || kind == Kind::Lead ? MotionStyle::Walk
                                                                 : MotionStyle::Dash;
            m.path.push_back(start);
            // Preserve the actual straight intermediate squares for two-cell dashes.
            if (!jump && !phase && (dx == 0 || dy == 0)) {
                int length = int(std::abs(dx) + std::abs(dy));
                for (int step = 1; step < length; ++step)
                    m.path.push_back(
                        {start.column + dx * step / length, start.row + dy * step / length});
            } else if (!jump && !phase) {
                // A missing/ambiguous historical route must never draw through a wall.
                m.style = MotionStyle::Phase;
            }
            m.path.push_back(finish);
            result.push_back(std::move(m));
        }
    }
    return result;
}

inline MotionSample sampleMovement(const Movement &m, double progress) {
    if (m.path.empty())
        return {};
    if (m.path.size() == 1)
        return {m.path.front().column, m.path.front().row};
    double t = std::clamp(progress, 0., 1.);
    const auto &a = m.path.front(), &b = m.path.back();
    if (m.style == MotionStyle::Phase) {
        const auto &point = t < .5 ? a : b;
        return {point.column, point.row, 0, std::abs(2 * t - 1), 0};
    }
    double eased = t * t * (3 - 2 * t);
    double segment = eased * (m.path.size() - 1);
    auto index = std::min(std::size_t(segment), m.path.size() - 2);
    double along = segment - index;
    const auto &p = m.path[index], &q = m.path[index + 1];
    return {p.column + (q.column - p.column) * along, p.row + (q.row - p.row) * along,
            m.style == MotionStyle::Leap ? std::sin(t * 3.141592653589793) * .42 : 0, 1,
            (q.column > p.column) - (q.column < p.column)};
}

struct Pose {
    double lean = 0, recoil = 0, flash = 0, opacity = 1, lift = 0;
    int facing = 0;
};

inline Pose actionPose(bool acting, bool hurt, bool defeated, double windup, double feedback,
                       int facing, bool full) {
    Pose pose;
    pose.facing = facing;
    if (acting && windup >= 0 && windup < 1 && full)
        pose.lean = std::sin(windup * 3.141592653589793) * .10;
    if (feedback >= 0 && feedback < 1) {
        if (hurt) {
            pose.flash = (1 - feedback) * .55;
            if (full)
                pose.recoil = std::sin(feedback * 3.141592653589793 * 2) * (1 - feedback) * .06;
        }
        if (defeated) {
            pose.opacity = 1 - feedback;
            pose.lift = full ? feedback * .12 : 0;
        }
    } else if (defeated)
        pose.opacity = 0;
    return pose;
}
} // namespace mist::presentation::sprites
