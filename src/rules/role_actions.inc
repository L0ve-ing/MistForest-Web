double armorUnload(double armor, int choice) { // 0=小数部分，1=整数一层或1+小数部分。
    double fraction = armor - std::floor(armor + 1e-9);
    if (fraction < 1e-9)
        fraction = 0;
    double amount = fraction + choice;
    if (fraction > 0 && choice > 1)
        return 0;
    return amount > 0 && amount <= armor + 1e-9 ? amount : 0;
}

int shadowTarget(Mode mode, int from, int dr, int dc);
bool shadowStep(Mode mode, int from, int to, const Map &map); // 环绕助手的前置声明。

int swordCap(const std::vector<Sword> &swords, int id) {
    int count = 0;
    for (const auto &x : swords)
        if (x.unit == id)
            ++count;
    return std::max(0, 9 - count);
} // 场上一剑减少一层剑气上限。

std::vector<int> swordLine(const View &v, int from, int direction,
                           bool includeSelf) { // 两格：前方两格，或自身加相邻格。
    std::vector<int> cells;
    int w = size(v.mode), dr = (direction - 1) / w - (from - 1) / w,
        dc = (direction - 1) % w - (from - 1) % w;
    if (distance(v.mode, from, direction) != 1)
        return cells;
    if (includeSelf)
        cells.push_back(from);
    int previous = from;
    for (int i = 0; i < (includeSelf ? 1 : 2); ++i) {
        int r = (previous - 1) / w + dr, c = (previous - 1) % w + dc;
        if (r < 0 || r >= rows(v.mode) || c < 0 || c >= w)
            break;
        int to = r * w + c + 1;
        if (!worldShot(v, previous, to))
            break;
        cells.push_back(to);
        if (v.buildings[to - 1].health > 0)
            break;
        previous = to;
    }
    return cells;
}

int validateV102(const View &v, const Action &a,
                 std::wstring *why) { // 新机制先判定，其余动作交给原有校验。
    const auto &u = v.units[a.unit];
    const auto &b = v.budget;
    auto no = [&](const wchar_t *reason) {
        if (why)
            *why = reason;
        return 0;
    };
    if (a.kind == Kind::ShadowMode)
        return u.role == Role::Shadow && b.active < 0 && !u.silenced &&
                       (a.amount == 1 || a.amount == 2)
                   ? 1
                   : no(L"附骨模式只能作为回合首个动作选择。");
    bool special = a.kind == Kind::PlantRose || a.kind == Kind::Bloom ||
                   a.kind == Kind::ThrowSword || a.kind == Kind::RecallSword;
    if (special && u.silenced)
        return no(L"沉默禁止该技能。");
    if (a.kind == Kind::PlantRose) { // 种植替代移动，不结束整回合；玫瑰可与单位、建筑同格。
        int count = 0;
        for (const auto &x : v.roses) {
            if (x.unit == a.unit && x.health > 0)
                ++count;
            if (x.cell == a.target && x.health > 0)
                return no(L"此格已有玫瑰。");
        }
        return u.role == Role::Prince && b.moves < 1 && count < 3 &&
                       valid(v.mode, a.target, v.map) &&
                       distance(v.mode, u.position, a.target) <= 2 &&
                       worldShot(v, u.position, a.target)
                   ? 1
                   : no(L"种植消耗移动额度，距离2，最多三朵。");
    }
    if (a.kind == Kind::Bloom) { // 攻击范围以瞬移落点为中心，使用限制按花朵身份记录。
        if (u.role != Role::Prince)
            return no(L"仅小王子可使用荆棘盛放。");
        if (a.target < 1 || a.target > cellCount(v.mode) || !canEnter(v, a.unit, a.landing))
            return no(L"请选择合法瞬移落点与攻击格。");
        for (const auto &x : v.roses)
            if (x.unit == a.unit && x.cell == a.extra && x.health > 0 &&
                std::find(b.roseUsed.begin(), b.roseUsed.end(), x.id) == b.roseUsed.end() &&
                distance(v.mode, x.cell, a.landing) <= 1 &&
                distance(v.mode, a.landing, a.target) <= 1 && worldShot(v, a.landing, a.target))
                return 1;
        return no(L"先选未使用的玫瑰，再选距离1内落点和落点距离1内攻击格。");
    }
    if (a.kind == Kind::ThrowSword) {
        int d = distance(v.mode, u.position, a.target), w = size(v.mode);
        bool straight = (u.position - 1) / w == (a.target - 1) / w ||
                        (u.position - 1) % w == (a.target - 1) % w;
        return u.role == Role::Sword && b.active < 0 && d >= 0 && d <= 3 && straight &&
                       worldShot(v, u.position, a.target) && (d == 0 || a.amount == d) &&
                       a.amount <= u.swordQi && swordCap(v.swords, a.unit) > 0
                   ? 1
                   : no(L"飞剑占整回合，可原地留剑或直线距离1至3，原地不消耗剑气，投出按距离消耗，放剑后可立即召回。");
    }
    if (a.kind == Kind::RecallSword) {
        if (u.role != Role::Sword)
            return no(L"仅长生剑可召回飞剑。");
        for (const auto &x : v.swords)
            if (x.unit == a.unit && x.cell == a.target &&
                distance(v.mode, u.position, x.cell) <= u.swordQi)
                return 1;
        return no(L"选择己方飞剑，剑气须足够支付当前距离。");
    }
    if (a.kind == Kind::SwordAdvance)
        return u.role == Role::Sword && b.swordAdvance &&
                       (a.target == 0 ||
                        (a.target >= 1 && a.target <= cellCount(v.mode) &&
                         b.swordCells[a.target - 1] && canEnter(v, a.unit, a.target)))
                   ? 1
                   : no(L"攻击后选择进入刚才剑刺的合法格，或留在原地。");
    if (u.role == Role::Prince && a.kind == Kind::Attack)
        return no(L"小王子没有普通攻击。");
    if (u.role == Role::Prince && a.kind == Kind::Move)
        return b.moves < 1 && worldMove(v, a.unit, u.position, a.target)
                   ? 1
                   : no(L"移动与种植共享一次移动额度。");
    if (u.role == Role::Sword && a.kind == Kind::Move)
        return (b.extraAttacks > 0 || (!b.swordBaseUsed && !b.moves && !b.attacks && !b.swordThrown)) && worldMove(v, a.unit, u.position, a.target)
                   ? 1
                   : no(L"普通移动与普攻互斥；回剑的额外行动可用于移动。");
    if (a.kind == Kind::SwordStab) {
        bool extra = b.extraAttacks > 0;
        return u.role == Role::Sword && u.swordQi > 0 &&
                       (extra || (!b.swordBaseUsed && !b.moves && !b.swordThrown)) &&
                       (a.amount == 0 || a.amount == 1) &&
                       !swordLine(v, u.position, a.target, a.amount != 0).empty()
                   ? 1
                   : no(L"剑刺需1剑气和普攻额度，点相邻格确定方向，可选择包含自身；消耗1剑气。");
    }
    if ((u.role == Role::Prince || u.role == Role::Sword) && a.kind == Kind::Attack)
        return no(L"请使用该职业专属攻击入口。");
    return -1;
}

void Game::startTurn() {
    resetDirectDamage(state_.turn);
    for (auto &f : state_.fires)
        if (f.expires && f.owner == state_.turn && state_.playerTurns[state_.turn] >= f.expires)
            f = {};
    for (int i = 0; i < 8; ++i) {
        auto &u = state_.units[i];
        if (u.controlSource == state_.turn && state_.playerTurns[state_.turn] >= u.controlExpires) {
            u.guard = u.silenced;
            u.silenced = u.vulnerable = false;
            u.controlSource = -1;
        }
        for (int k = 0; k < 3; ++k)
            if (u.soldierControlSource[k] == state_.turn &&
                state_.playerTurns[state_.turn] >= u.soldierControlExpires[k]) {
                u.soldierGuard[k] = u.soldierSilenced[k];
                u.soldierSilenced[k] = u.soldierVulnerable[k] = false;
                u.soldierControlSource[k] = -1;
            }
        if (i / 2 != state_.turn)
            continue;
        u.shield = 0;
        u.health = std::max(0.0, u.health - u.temporaryHealth);
        u.temporaryHealth = 0;
        if (u.role == Role::Shadow) {
            u.guard = u.silenced;
            u.silenced = false;
        }
    }
} // 上限和临时血量按受击方计时，游猎控制按施放方计时。

void Game::copyLeap(int id, int dest) { // 日字跟随分为前进两格和侧移一格，分别反馈。
    int w = size(state_.mode), from = state_.units[id].position,
        dr = (dest - 1) / w - (from - 1) / w, dc = (dest - 1) % w - (from - 1) % w;
    int fr = std::abs(dr) == 2 ? dr : 0, fc = std::abs(dc) == 2 ? dc : 0, sr = dr - fr,
        sc = dc - fc;
    for (int j = 0; j < 8; ++j) {
        auto &u = state_.units[j];
        if (u.health <= 0 || u.follow != id || u.silenced || immune(u) || immune(state_.units[id]))
            continue;
        int segment = 0;
        for (auto part : {std::pair<int, int>{fr, fc}, {sr, sc}}) {
            int from = u.position;
            int to = u.role == Role::Shadow
                         ? shadowTarget(state_.mode, from, part.first, part.second)
                         : 0;
            if (!to) {
                int row = (from - 1) / w + part.first, col = (from - 1) % w + part.second;
                if (row >= 0 && row < rows(state_.mode) && col >= 0 && col < w)
                    to = row * w + col + 1;
            }
            bool allowed = to && canEnter(view(j / 2), j, to);
            if (segment == 0) {
                int leg = u.role == Role::Shadow
                              ? shadowTarget(state_.mode, from, part.first / 2, part.second / 2)
                              : from + (part.first / 2) * w + part.second / 2;
                Map edges = state_.map;
                edges.cells.fill(Terrain::Ground);
                allowed = allowed && canEnter(view(j / 2), j, leg) &&
                          (u.role == Role::Shadow ? shadowStep(state_.mode, from, leg, edges)
                                                  : canMove(state_.mode, from, leg, edges));
            }
            if (allowed) {
                u.position = to;
            }
            u.report += allowed ? L"\n日字跟随片段：未受阻。" : L"\n日字跟随片段：受阻。";
            ++segment;
        }
        for (int p = 0; p < 4; ++p)
            resetCandidates(p, j);
    }
}

bool Game::applyV102(const Action &a, Event &e) { // 原回合裁判中的新增动作分发。
    auto &u = state_.units[a.unit];
    auto &b = state_.budget;
    if (a.kind == Kind::ShadowMode) {
        b.shadowMode = a.amount;
        e.publicText += a.amount == 1 ? L"本回合选择主动带动" : L"本回合选择指定跟随";
        return true;
    }
    if (a.kind == Kind::PlantRose) {
        state_.roses.push_back({a.unit, a.target, 3, int(state_.roses.size()) + 1});
        ++b.moves;
        ++b.used;
        e.attackCells[a.target - 1] = true;
        e.publicText +=
            L"玫瑰之吻" + coordinate(state_.mode, a.target) + L"，替代移动并造成0.5伤害";
        damage(a.unit, a.target, false, false, e, false, 1);
        return true;
    }
    if (a.kind == Kind::Bloom) {
        auto rose = std::find_if(state_.roses.begin(), state_.roses.end(), [&](const Rose &x) {
            return x.unit == a.unit && x.cell == a.extra && x.health > 0;
        });
        b.roseUsed.push_back(rose->id);
        rose->health = std::max(0.0, rose->health - 1);
        e.publicText += L"荆棘盛放，玫瑰" + coordinate(state_.mode, a.extra) + L"掉落花瓣，余" +
                        halfPoints(rose->health);
        moveUnit(a.unit, a.landing, false, false, -1, 0, true);
        e.attackCells[a.target - 1] = true;
        e.publicText += L"\n荆棘盛放攻击" + coordinate(state_.mode, a.target);
        e.privateText = L"瞬移到" + coordinate(state_.mode, a.landing);
        damage(a.unit, a.target, false, false, e);
        return true;
    }
    if (a.kind == Kind::ThrowSword) {
        int cost = distance(state_.mode, u.position, a.target);
        u.swordQi -= cost;
        state_.swords.push_back({a.unit, a.target, cost});
        b.swordThrown = b.swordBaseUsed = true;
        ++b.used;
        e.attackCells[a.target - 1] = true;
        e.publicText += L"剑来·飞剑" + coordinate(state_.mode, a.target);
        e.privateText = L"消耗剑气" + std::to_wstring(cost);
        damage(a.unit, a.target, false, false, e);
        return true; // 正常行动已用完，仍保留立即回剑及其额外行动。
    }
    if (a.kind == Kind::RecallSword) {
        auto it = std::find_if(state_.swords.begin(), state_.swords.end(), [&](const Sword &x) {
            return x.unit == a.unit && x.cell == a.target;
        });
        u.swordQi -= distance(state_.mode, u.position, it->cell);
        state_.swords.erase(it);
        ++b.extraAttacks;
        e.publicText += L"剑来·召回" + coordinate(state_.mode, a.target);
        return true;
    }
    if (u.role == Role::Sword && a.kind == Kind::Move) {
        if (!b.swordBaseUsed && !b.moves && !b.attacks && !b.swordThrown)
            b.swordBaseUsed = true;
        else
            --b.extraAttacks;
        moveUnit(a.unit, a.target, false, false);
        ++b.moves;
        ++b.used;
        e.publicText += L"移动";
        e.privateText = L"移动到" + coordinate(state_.mode, a.target);
        return true;
    }
    if (a.kind == Kind::SwordStab) {
        auto cells = swordLine(view(state_.turn), u.position, a.target, a.amount != 0);
        --u.swordQi;
        if (!b.swordBaseUsed && !b.moves && !b.swordThrown)
            b.swordBaseUsed = true;
        else
            --b.extraAttacks;
        ++b.attacks;
        ++b.used;
        b.swordCells = {};
        b.swordAdvance = true;
        for (int c : cells) {
            b.swordCells[c - 1] = true;
            e.attackCells[c - 1] = true;
            e.publicText += L"\n剑刺" + coordinate(state_.mode, c);
            damage(a.unit, c, false, false, e);
        }
        return true;
    }
    if (a.kind == Kind::SwordAdvance) {
        b.swordAdvance = false;
        if (a.target == 0) {
            e.publicText += L"攻击后选择完成";
            e.privateText = L"留在原地。";
            return true;
        }
        int w = size(state_.mode), from = u.position, dr = (a.target - 1) / w - (from - 1) / w,
            dc = (a.target - 1) % w - (from - 1) % w;
        int steps = std::abs(dr) + std::abs(dc);
        for (int k = 0; k < steps; ++k)
            moveUnit(a.unit, u.position + (dr ? (dr > 0 ? w : -w) : (dc > 0 ? 1 : -1)), false,
                     false);
        e.publicText += L"攻击后选择完成";
        e.privateText = L"进入攻击格" + coordinate(state_.mode, a.target);
        return true;
    }
    return false;
}

Map randomMap(int width, int height,
              unsigned seed) { // 固定种子可复现，阻挡地形与线墙均进行连通检查。
    Map map;
    map.width = width;
    map.height = height;
    if (width < 1 || width > 9 || height < 1 || height > 9)
        throw std::invalid_argument("map size");
    Mode mode = mapMode(map);
    std::mt19937 rng(seed);
    auto connected = [&]() {
        Mask seen{};
        std::vector<int> todo;
        for (int c = 1; c <= cellCount(mode); ++c)
            if (valid(mode, c, map)) {
                todo.push_back(c);
                seen[c - 1] = true;
                break;
            }
        if (todo.empty())
            return false;
        for (std::size_t i = 0; i < todo.size(); ++i)
            for (int c = 1; c <= cellCount(mode); ++c)
                if (!seen[c - 1] && canMove(mode, todo[i], c, map)) {
                    seen[c - 1] = true;
                    todo.push_back(c);
                }
        for (int c = 1; c <= cellCount(mode); ++c)
            if (valid(mode, c, map) && !seen[c - 1])
                return false;
        return true;
    };
    for (int c = 0; c < width * height; ++c) {
        unsigned roll = rng() % 100;
        if (roll < 22 && width * height > 4) {
            map.cells[c] = roll < 13 ? Terrain::Wall : Terrain::Lake;
            if (!connected())
                map.cells[c] = Terrain::Ground;
        } else if (roll < 29)
            map.cells[c] = Terrain::Whirlpool;
    }
    for (int r = 0; r < height; ++r)
        for (int c = 1; c < width; ++c)
            if (rng() % 100 < 8) {
                int i = r * (width + 1) + c;
                map.vertical[i] = true;
                if (!connected())
                    map.vertical[i] = false;
            }
    for (int r = 1; r < height; ++r)
        for (int c = 0; c < width; ++c)
            if (rng() % 100 < 8) {
                int i = r * width + c;
                map.horizontal[i] = true;
                if (!connected())
                    map.horizontal[i] = false;
            }
    return map;
}
