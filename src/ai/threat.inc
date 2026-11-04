// Next-turn damage coverage, from a hypothetical candidate origin. Public
// preparations are considered; this is a heuristic, not an adversarial solver.
// Geometry uses the same terrain, building and ray helpers as the referee.
Mask botCoverage(const View &v, int id, int origin, int piece = 0) {
    Mask coverage{};
    View w = v;
    w.player = id / 2;
    w.turn = w.player;
    w.budget = {};
    w.winner = -1;
    auto &actor = w.units[id];
    actor.position = origin;
    actor.soldiers[piece] = origin;
    actor.silenced = false;
    actor.soldierSilenced = {};
    w.budget.shadowMode = 2;
    auto merge = [&](const Mask &mask) {
        for (int c = 0; c < 81; ++c)
            coverage[c] = coverage[c] || mask[c];
    };
    auto direct = [&](const View &state, int from) {
        const auto &u = state.units[id];
        for (int t = 1; t <= cellCount(v.mode); ++t) {
            Kind kind = u.role == Role::Armiger  ? Kind::Thrust
                        : u.role == Role::Flamer ? Kind::Flame
                        : u.role == Role::Sword  ? Kind::SwordStab
                                                 : Kind::Attack;
            Action a{id, kind, t, -1, piece, 0, u.role == Role::Flamer ? std::min(3, u.ammo) : 1};
            if (validate(state, a)) {
                if (kind == Kind::Thrust || kind == Kind::Flame)
                    for (int c : rayCells(state, from, t,
                                          kind == Kind::Flame ? a.amount
                                                              : 1 + int(std::ceil(u.armor - 1e-9))))
                        coverage[c - 1] = true;
                else if (kind == Kind::SwordStab) {
                    for (int c : swordLine(state, from, t, false))
                        coverage[c - 1] = true;
                    for (int c : swordLine(state, from, t, true))
                        coverage[c - 1] = true;
                } else
                    coverage[t - 1] = true;
            }
            if (u.role == Role::Sword) {
                a.kind = Kind::ThrowSword;
                a.amount = distance(v.mode, from, t);
                if (validate(state, a))
                    coverage[t - 1] = true;
            }
        }
    };
    if (!heroMode(v.mode)) {
        direct(w, origin);
        return coverage;
    }
    if (actor.role == Role::Heavy) {
        const int width = size(v.mode);
        const std::array<std::pair<int, int>, 4> directions{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
        auto step = [&](int c, std::pair<int, int> d) {
            int r = (c - 1) / width + d.first, col = (c - 1) % width + d.second;
            return r < 0 || r >= rows(v.mode) || col < 0 || col >= width ? 0 : r * width + col + 1;
        };
        for (auto first : directions) {
            int p = step(origin, first);
            if (!p || !worldMove(w, id, origin, p))
                continue;
            for (auto direction : directions) {
                if (direction.first == -first.first && direction.second == -first.second)
                    continue;
                int previous = p;
                for (int k = 2; k <= 4; ++k) {
                    int t = step(previous, direction);
                    if (!t || !canMove(v.mode, previous, t, v.map))
                        break;
                    coverage[t - 1] = true;
                    if (!canEnter(w, id, t))
                        break;
                    previous = t;
                }
            }
        }
        return coverage;
    }
    if (actor.role == Role::Knight) {
        for (int landing = 1; landing <= cellCount(v.mode); ++landing)
            if (validate(w, {id, Kind::Hunt, landing})) {
                coverage[landing - 1] = true;
                View next = w;
                next.units[id].position = landing;
                next.budget.active = id;
                next.budget.used = 1;
                next.budget.phase = 3;
                next.budget.anchor = landing;
                for (int t = 1; t <= cellCount(v.mode); ++t)
                    if (validate(next, {id, Kind::Cleave, t}))
                        coverage[t - 1] = true;
            }
        return coverage;
    }
    if (actor.role == Role::Prince) {
        std::vector<View> origins{w};
        for (const auto &rose : w.roses)
            if (rose.unit == id && rose.health > 0)
                for (int c = 1; c <= cellCount(v.mode); ++c)
                    if (distance(v.mode, rose.cell, c) <= 1 && canEnter(w, id, c)) {
                        View next = w;
                        next.units[id].position = c;
                        for (auto &used : next.roses)
                            if (used.id == rose.id)
                                used.health = std::max(0.0, used.health - 1);
                        origins.push_back(std::move(next));
                    }
        Mask roseSites{};
        for (const auto &rose : w.roses)
            if (rose.unit == id && rose.health > 0)
                roseSites[rose.cell - 1] = true;
        for (const auto &next : origins) {
            for (int site = 1; site <= cellCount(v.mode); ++site)
                if (validate(next, {id, Kind::PlantRose, site})) {
                    coverage[site - 1] = true;
                    roseSites[site - 1] = true;
                }
        }
        for (int site = 1; site <= cellCount(v.mode); ++site)
            if (roseSites[site - 1])
                for (int landing = 1; landing <= cellCount(v.mode); ++landing)
                    if (distance(v.mode, site, landing) <= 1 && canEnter(w, id, landing))
                        for (int t = 1; t <= cellCount(v.mode); ++t)
                            if (distance(v.mode, landing, t) <= 1 && worldShot(w, landing, t))
                                coverage[t - 1] = true;
        return coverage;
    }
    direct(w, origin);
    if (actor.role == Role::Wolf || actor.role == Role::Ink || actor.role == Role::Armiger ||
        (actor.role == Role::HiddenWolf && actor.form)) {
        // Wolf can spend two of its three actions before biting. Ink has one
        // ordinary move and a separate two-step Dash; armor unloading is free.
        std::vector<View> states{w};
        int layers = actor.role == Role::Wolf      ? 2
                     : actor.role == Role::Ink     ? 3
                     : actor.role == Role::Armiger ? 2
                                                   : 1;
        Mask reached{};
        reached[origin - 1] = true;
        for (int layer = 0; layer < layers; ++layer) {
            std::vector<View> nextStates;
            for (const auto &state : states) {
                int from = state.units[id].position;
                View moving = state;
                if (actor.role == Role::Armiger)
                    moving.units[id].armor = 0;
                for (int t = 1; t <= cellCount(v.mode); ++t)
                    if (t != from && worldMove(moving, id, from, t) && !reached[t - 1]) {
                        View next = moving;
                        next.units[id].position = t;
                        next.budget.active = id;
                        next.budget.moves = layer + 1;
                        next.budget.used = layer + 1;
                        if (actor.role == Role::Ink) {
                            if (layer >= 1 && actor.cooldown > 0)
                                continue;
                            next.budget.moves = 1;
                            next.budget.used = 1;
                        }
                        reached[t - 1] = true;
                        direct(next, t);
                        nextStates.push_back(next);
                    }
            }
            states = std::move(nextStates);
        }
        if (actor.role == Role::Armiger && actor.armor > 0) {
            View unloaded = w;
            unloaded.units[id].armor = 0;
            direct(unloaded, origin);
        }
    }
    if (actor.role == Role::HiddenWolf && actor.cooldown == 0) {
        View changed = v;
        changed.units[id].form = 1 - actor.form;
        changed.units[id].cooldown = 1;
        merge(botCoverage(changed, id, origin, piece));
    }
    if (actor.role == Role::Flamer) {
        std::vector<int> origins{origin};
        for (int c = 1; c <= cellCount(v.mode); ++c)
            if (w.fires[c - 1].expires && canEnter(w, id, c))
                origins.push_back(c);
        for (int from : origins) {
            View next = w;
            next.units[id].position = from;
            direct(next, from);
            if (actor.ammo > 0)
                for (int t = 1; t <= cellCount(v.mode); ++t)
                    if (t != from && worldMove(next, id, from, t)) {
                        coverage[t - 1] = true;
                        View stepped = next;
                        stepped.units[id].position = t;
                        --stepped.units[id].ammo;
                        stepped.budget.fireStep = true;
                        stepped.budget.active = id;
                        direct(stepped, t);
                    }
        }
    }
    if (actor.role == Role::Sword &&
        std::any_of(w.swords.begin(), w.swords.end(),
                    [&](const Sword &sword) { return sword.unit == id; })) {
        // A stored sword can fund a Move + extra Stab, or Stab + Advance +
        // another Stab. No stored sword means the diagonal is not threatened.
        for (const auto &sword : w.swords)
            if (sword.unit == id) {
                int cost = distance(v.mode, origin, sword.cell);
                if (actor.swordQi - cost < 1)
                    continue;
                View recalled = w;
                recalled.units[id].swordQi -= cost;
                recalled.budget.extraAttacks = 1;
                for (int t = 1; t <= cellCount(v.mode); ++t)
                    if (worldMove(recalled, id, origin, t)) {
                        View next = recalled;
                        next.units[id].position = t;
                        next.budget.active = id;
                        next.budget.moves = 1;
                        direct(next, t);
                    }
                if (actor.swordQi - cost >= 2)
                    for (int direction = 1; direction <= cellCount(v.mode); ++direction)
                        for (int landing : swordLine(w, origin, direction, false))
                            if (canEnter(w, id, landing)) {
                                View next = recalled;
                                --next.units[id].swordQi;
                                next.units[id].position = landing;
                                next.budget.active = id;
                                next.budget.attacks = 1;
                                next.budget.swordBaseUsed = true;
                                direct(next, landing);
                            }
            }
        // Moving or advancing first can make a previously unaffordable recall
        // affordable. Its resulting attack still needs one remaining Qi.
        for (int landing = 1; landing <= cellCount(v.mode); ++landing) {
            bool move = worldMove(w, id, origin, landing);
            bool advance = false;
            if (actor.swordQi > 0)
                for (int direction = 1; direction <= cellCount(v.mode); ++direction) {
                    auto line = swordLine(w, origin, direction, false);
                    if (std::find(line.begin(), line.end(), landing) != line.end() &&
                        canEnter(w, id, landing))
                        advance = true;
                }
            for (const auto &sword : w.swords)
                if (sword.unit == id) {
                    int cost = distance(v.mode, landing, sword.cell);
                    if (move && actor.swordQi - cost >= 1) {
                        View next = w;
                        next.units[id].position = landing;
                        next.units[id].swordQi -= cost;
                        next.budget.active = id;
                        next.budget.moves = 1;
                        next.budget.extraAttacks = 1;
                        direct(next, landing);
                    }
                    if (advance && actor.swordQi - cost >= 2) {
                        View next = w;
                        next.units[id].position = landing;
                        next.units[id].swordQi -= cost + 1;
                        next.budget.active = id;
                        next.budget.swordBaseUsed = true;
                        next.budget.attacks = 1;
                        next.budget.extraAttacks = 1;
                        direct(next, landing);
                    }
                }
        }
    }
    return coverage;
}

void botPrepareThreat(const View &v, BotKnowledge &knowledge) {
    View publicView = v;
    publicView.notices.clear();
    publicView.result.clear();
    publicView.turnReport.clear();
    publicView.previousReport.clear();
    publicView.skillFeedback.clear();
    for (int id = 0; id < v.playerCount * 2; ++id)
        if (v.units[id].health > 0 && !allied(v.teams, id / 2, v.player))
            for (int k = 0; k < (v.units[id].role == Role::WallGuard ? 3 : 1); ++k) {
                if (v.units[id].role == Role::WallGuard && v.units[id].soldierHealth[k] <= 0)
                    continue;
                for (int from = 1; from <= cellCount(v.mode); ++from) {
                    double probability = knowledge.pieces[id][k][from - 1];
                    if (probability <= 0)
                        continue;
                    auto mask = botCoverage(publicView, id, from, k);
                    for (int c = 0; c < cellCount(v.mode); ++c)
                        if (mask[c])
                            knowledge.threat[id][c] += probability;
                }
            }
}
