// Reconstruct beliefs from the observer's sanitized notice history. No global
// cache: separate games, replays and reconnects cannot share private knowledge.
using BotDistribution = std::array<double, 81>;

struct BotKnowledge {
    std::array<BotDistribution, 8> probability{};
    std::array<std::array<BotDistribution, 3>, 8> pieces{};
    std::array<std::array<double, 81>, 8> threat{};
};

std::vector<int> botCoordinates(const View &v, const std::wstring &text) {
    std::vector<int> result;
    for (int c = 1; c <= cellCount(v.mode); ++c)
        if (text.find(coordinate(v.mode, c)) != std::wstring::npos)
            result.push_back(c);
    return result;
}

void botNormalize(BotDistribution &p) {
    double total = 0;
    for (double x : p)
        total += x;
    if (total > 0)
        for (double &x : p)
            x /= total;
}

BotDistribution botUniform(const View &v) {
    BotDistribution p{};
    for (int c = 1; c <= cellCount(v.mode); ++c)
        if (valid(v.mode, c, v.map))
            p[c - 1] = 1;
    botNormalize(p);
    return p;
}

void botRestrict(BotDistribution &p, const Mask &cells) {
    BotDistribution next{};
    for (int c = 0; c < 81; ++c)
        if (cells[c])
            next[c] = p[c];
    double total = 0;
    for (double x : next)
        total += x;
    if (total == 0)
        for (int c = 0; c < 81; ++c)
            if (cells[c])
                next[c] = 1;
    botNormalize(next);
    if (std::any_of(next.begin(), next.end(), [](double x) { return x > 0; }))
        p = next;
}

void botExpand(const View &v, int id, BotDistribution &p, int steps, bool leap = false,
               bool stay = false) {
    // Historical buildings may since have changed. Static terrain is safe;
    // current public candidate masks are applied after replay.
    for (int k = 0; k < steps; ++k) {
        BotDistribution next{};
        for (int from = 1; from <= cellCount(v.mode); ++from)
            if (p[from - 1] > 0) {
                std::vector<int> cells;
                if (stay)
                    cells.push_back(from);
                for (int to = 1; to <= cellCount(v.mode); ++to) {
                    bool legal = leap ? (from == to || canLeap(v.mode, from, to, v.map))
                                 : v.units[id].role == Role::Shadow
                                     ? shadowStep(v.mode, from, to, v.map)
                                     : canMove(v.mode, from, to, v.map);
                    if (legal)
                        cells.push_back(to);
                }
                if (cells.empty())
                    cells.push_back(from);
                for (int to : cells)
                    next[to - 1] += p[from - 1] / cells.size();
            }
        p = next;
    }
}

void botWhirl(const View &v, BotDistribution &p) {
    BotDistribution next = p;
    for (int c = 1; c <= cellCount(v.mode); ++c)
        if (p[c - 1] > 0)
            for (int t = 1; t <= cellCount(v.mode); ++t)
                if (v.map.cells[t - 1] == Terrain::Whirlpool && distance(v.mode, c, t) == 1 &&
                    canMove(v.mode, c, t, v.map)) {
                    next[t - 1] += p[c - 1];
                    break;
                }
    p = next;
    botNormalize(p); // Immunity is private: both staying and pulling remain possible.
}

BotKnowledge botKnowledge(const View &v) {
    BotKnowledge result;
    auto uniform = botUniform(v);
    for (auto &unit : result.pieces)
        for (auto &p : unit)
            p = uniform;
    std::array<int, 4> speakers{};
    speakers.fill(-1);
    std::array<Mask, 8> swordCells{};
    std::array<int, 8> swordRevision{};
    swordRevision.fill(-1);
    std::array<bool, 8> following{};
    std::array<bool, 8> historicalAlive{};
    std::array<int, 8> historicalForm{};
    for (int id = 0; id < v.playerCount * 2; ++id)
        historicalAlive[id] = id % 2 < v.teamSize;
    int previousTurn = -1, revision = -1;
    bool collision = false, burning = false;
    int roseCell = 0;
    auto enemy = [&](int id) { return id >= 0 && id < 8 && !allied(v.teams, id / 2, v.player); };
    auto expand = [&](int id, int steps, bool leap = false, bool stay = false, int piece = -1) {
        if (!enemy(id))
            return;
        for (int k = 0; k < (v.units[id].role == Role::WallGuard ? 3 : 1); ++k)
            if (piece < 0 || piece == k)
                botExpand(v, id, result.pieces[id][k], steps, leap, stay);
    };
    auto restrictUnit = [&](int id, const Mask &cells, int piece = -1) {
        if (!enemy(id))
            return;
        for (int k = 0; k < (v.units[id].role == Role::WallGuard ? 3 : 1); ++k)
            if (piece < 0 || piece == k)
                botRestrict(result.pieces[id][k], cells);
    };
    for (const auto &n : v.notices) {
        if (n.turn < 0 || n.player < 0 || n.player >= v.playerCount)
            continue;
        // A turn boundary may include a pull. Applying at each boundary is
        // deliberately conservative when eliminated players shorten a round.
        if (previousTurn >= 0 && n.turn != previousTurn)
            for (int id = 0; id < v.playerCount * 2; ++id)
                if (enemy(id))
                    for (auto &p : result.pieces[id])
                        botWhirl(v, p);
        previousTurn = n.turn;
        if (n.revision != revision) {
            revision = n.revision;
            collision = false;
            roseCell = 0;
            burning = false;
        }
        if (n.text.find(L"燃烧结算") == 0)
            burning = true;
        int actor = -1;
        bool title = false;
        for (int id = n.player * 2; id < n.player * 2 + v.teamSize; ++id)
            if (n.text.find(roleName(v.units[id].role) + L" · ") == 0) {
                actor = id;
                speakers[n.player] = id;
                title = true;
                break;
            }
        if (actor < 0)
            actor = speakers[n.player];
        if (title && n.text.find(L"变为狼形") != std::wstring::npos)
            historicalForm[actor] = 1;
        if (title && n.text.find(L"变为人形") != std::wstring::npos)
            historicalForm[actor] = 0;
        auto coords = botCoordinates(v, n.text);
        int piece = -1;
        for (int k = 0; k < 3; ++k)
            if (n.text.find(L"士兵" + std::to_wstring(k + 1)) != std::wstring::npos)
                piece = k;
        bool dash = title && n.text.find(L"炫空斗舞") != std::wstring::npos &&
                    n.text.find(L"霰弹") == std::wstring::npos;
        bool movement = title && (n.text.find(L"移动") != std::wstring::npos ||
                                  n.text.find(L"附骨带动") != std::wstring::npos || dash ||
                                  n.text.find(L"冲锋推进") != std::wstring::npos ||
                                  n.text.find(L"冲锋撞毁") != std::wstring::npos ||
                                  n.text.find(L"日字") != std::wstring::npos ||
                                  n.text.find(L"秘密跳跃") != std::wstring::npos ||
                                  n.text.find(L"火步") != std::wstring::npos ||
                                  n.text.find(L"进入火焰") != std::wstring::npos);
        if (movement) {
            bool leap = n.text.find(L"日字") != std::wstring::npos ||
                        n.text.find(L"秘密跳跃") != std::wstring::npos;
            int steps = dash ? 2 : 1;
            bool blink = n.text.find(L"进入火焰") != std::wstring::npos;
            if (blink && enemy(actor)) {
                for (auto &p : result.pieces[actor])
                    p = uniform;
            } // Historical fire may have expired.
            else
                expand(actor, steps, leap, n.text.find(L"冲锋撞毁") != std::wstring::npos, piece);
            for (int id = 0; id < v.playerCount * 2; ++id)
                if (enemy(id) && following[id] && id != actor) {
                    if (blink || leap)
                        result.pieces[id][0] = uniform;
                    else
                        expand(id, steps, false, true);
                }
            if (n.text.find(L"附骨带动") != std::wstring::npos)
                for (int id = 0; id < v.playerCount * 2; ++id)
                    if (id != actor &&
                        n.text.find(roleName(v.units[id].role)) != std::wstring::npos)
                        expand(id, 1, false, true);
            collision = n.text.find(L"冲锋撞毁") != std::wstring::npos;
            if (n.text.find(L"火步并点燃") != std::wstring::npos && coords.size() == 1) {
                Mask exact{};
                exact[coords[0] - 1] = true;
                restrictUnit(actor, exact);
            }
        }
        if (title && n.text.find(L"附骨指定") != std::wstring::npos && enemy(actor))
            following[actor] = true;
        if (title && actor >= 0 && actor < 8 && n.text.find(L"剑刺后位移") != std::wstring::npos) {
            if (enemy(actor))
                result.pieces[actor][0] = uniform;
            restrictUnit(actor, swordCells[actor]);
        }
        if (title && n.text.find(L"剑刺后位移") != std::wstring::npos)
            for (int id = 0; id < v.playerCount * 2; ++id)
                if (enemy(id) && following[id] && id != actor)
                    expand(id, 2, false, true);
        if (title && n.text.find(L"攻击后选择完成") != std::wstring::npos) {
            if (enemy(actor)) {
                if (n.kind == Kind::SwordAdvance) {
                    auto &p = result.pieces[actor][0];
                    for (int c = 1; c <= cellCount(v.mode); ++c)
                        if (swordCells[actor][c - 1])
                            p[c - 1] += uniform[c - 1];
                    botNormalize(p);
                } else if (n.kind == Kind::FlameAdvance)
                    result.pieces[actor][0] = uniform; // Earlier fire can expire.
            }
            for (int id = 0; id < v.playerCount * 2; ++id)
                if (enemy(id) && following[id] && id != actor)
                    result.pieces[id][0] = uniform;
        }
        bool rush =
            title &&
            ((v.units[actor].role == Role::Alpha && n.text.find(L"攻击") != std::wstring::npos) ||
             n.text.find(L"游猎②挥劈") != std::wstring::npos);
        if (rush) {
            expand(actor, 1, false, true);
            for (int id = 0; id < v.playerCount * 2; ++id)
                if (enemy(id) && following[id] && id != actor)
                    expand(id, 1, false, true);
        } // Public attacks do not distinguish Rush; it also copies to followers.
        if (title && n.text.find(L"嗜血：自身瞬间位置") != std::wstring::npos &&
            coords.size() == 1) {
            Mask exact{};
            exact[coords[0] - 1] = true;
            restrictUnit(actor, exact);
        }
        if (title && n.text.find(L"倾轧落点") != std::wstring::npos && coords.size() == 1) {
            Mask exact{};
            exact[coords[0] - 1] = true;
            restrictUnit(actor, exact);
            for (int id = 0; id < v.playerCount * 2; ++id)
                if (id != actor && following[id])
                    result.pieces[id][0] = uniform;
        }
        if (title && n.text.find(L"荆棘盛放，玫瑰") != std::wstring::npos && !coords.empty())
            roseCell = coords[0];
        if (n.text.find(L"荆棘盛放攻击") == 0 && roseCell && !coords.empty()) {
            Mask landing{};
            for (int c = 1; c <= cellCount(v.mode); ++c)
                landing[c - 1] = valid(v.mode, c, v.map) && distance(v.mode, c, roseCell) <= 1 &&
                                 distance(v.mode, c, coords[0]) <= 1;
            if (enemy(actor))
                result.pieces[actor][0] = uniform;
            restrictUnit(actor, landing);
            for (int id = 0; id < v.playerCount * 2; ++id)
                if (id != actor && following[id])
                    result.pieces[id][0] = uniform;
        }
        if (actor >= 0 && actor < 8 && n.text.find(L"剑刺") == 0) {
            if (swordRevision[actor] != n.revision) {
                swordCells[actor] = {};
                swordRevision[actor] = n.revision;
            }
            for (int c : coords)
                swordCells[actor][c - 1] = true;
        }
        if (n.text.find(L"冲锋判定格：") == 0 && enemy(actor) && !coords.empty()) {
            Mask endpoints{};
            int w = size(v.mode);
            bool straight = true;
            int delta = coords.size() > 1 ? coords[1] - coords[0] : 1;
            if (delta != 1 && delta != w)
                straight = false;
            for (std::size_t k = 1; k < coords.size(); ++k)
                if (coords[k] - coords[k - 1] != delta ||
                    (delta == 1 && (coords[k] - 1) / w != (coords[0] - 1) / w))
                    straight = false;
            if (straight && coords.size() <= 3) {
                endpoints[coords.front() - 1] = true;
                endpoints[coords.back() - 1] = true;
                if (collision)
                    for (int c : coords)
                        if (v.buildings[c - 1].health > 0)
                            for (int t = 1; t <= cellCount(v.mode); ++t)
                                if (canMove(v.mode, c, t, v.map))
                                    endpoints[t - 1] = true;
                restrictUnit(actor, endpoints);
            }
        }
        bool hit = n.text.find(L"受击 · ") == 0 || n.text.find(L"击杀 · ") == 0;
        if (hit) {
            int victim = -1, matches = 0;
            for (int id = 0; id < v.playerCount * 2; ++id)
                if (enemy(id) && (!burning || id / 2 == n.player) &&
                    n.text.find(roleName(v.units[id].role)) != std::wstring::npos) {
                    victim = id;
                    ++matches;
                }
            if (matches == 1 && std::count(n.attacks.begin(), n.attacks.end(), true) == 1)
                restrictUnit(victim, n.attacks, piece);
            if (matches == 1 && n.text.find(L"击杀 · ") == 0 &&
                v.units[victim].role != Role::WallGuard)
                historicalAlive[victim] = false;
        }
        if (n.player == v.player && n.text.find(L"最近敌方单位瞬间位置") != std::wstring::npos &&
            coords.size() == 1) {
            std::vector<std::pair<int, int>> eligible;
            for (int id = 0; id < v.playerCount * 2; ++id)
                if (enemy(id) && historicalAlive[id] &&
                    (v.units[id].role != Role::HiddenWolf || historicalForm[id]))
                    for (int k = 0; k < (v.units[id].role == Role::WallGuard ? 3 : 1); ++k)
                        eligible.push_back({id, k});
            if (eligible.size() == 1) {
                Mask exact{};
                exact[coords[0] - 1] = true;
                botRestrict(result.pieces[eligible[0].first][eligible[0].second], exact);
            } else
                for (auto x : eligible) {
                    auto &p = result.pieces[x.first][x.second];
                    p[coords[0] - 1] += 1.0 / std::max(std::size_t(1), eligible.size());
                    botNormalize(p);
                } // Detection gives no identity: retain a mixture.
        }
    }
    if (previousTurn >= 0 && v.turns != previousTurn)
        for (int id = 0; id < v.playerCount * 2; ++id)
            if (enemy(id))
                for (auto &p : result.pieces[id])
                    botWhirl(v, p);
    for (int id = 0; id < v.playerCount * 2; ++id)
        if (enemy(id) && v.units[id].health > 0) {
            int count = 0;
            for (int k = 0; k < (v.units[id].role == Role::WallGuard ? 3 : 1); ++k) {
                if (v.units[id].role == Role::WallGuard && v.units[id].soldierHealth[k] <= 0)
                    continue;
                auto &p = result.pieces[id][k];
                botRestrict(p, v.possible[id]);
                ++count;
                for (int c = 0; c < 81; ++c)
                    result.probability[id][c] += p[c];
            }
            if (count)
                for (double &p : result.probability[id])
                    p /= count;
        }
    return result;
}
