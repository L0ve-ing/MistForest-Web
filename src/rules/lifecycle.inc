void Game::resetDirectDamage(int player) {
    for (int i = player * 2; i < player * 2 + 2; ++i) {
        state_.units[i].directDamage = 0;
        state_.units[i].soldierDirectDamage = {};
    }
    for (auto &b : state_.buildings)
        if (b.owner == player)
            b.directDamage = 0;
    for (auto &r : state_.roses)
        if (r.unit / 2 == player)
            r.directDamage = 0;
}

double initialHealth(Role role, Mode mode) {
    return !heroMode(mode) || role == Role::Sniper ? 2
           : role == Role::Heavy                   ? 4
           : role == Role::WallGuard               ? 4.8
                                                   : 3;
}

Game::Game(std::array<int, 8> positions, std::array<Role, 8> roles, Mode mode, int first, Map map,
           int teamSize, Formations formations, int playerCount,
           bool teams) { // 最多四玩家、每人最多两角色。
    if (!validMap(mode, map) || playerCount < 2 || playerCount > 4 || first < 0 ||
        first >= playerCount || teamSize < 1 || teamSize > 2 || (teams && playerCount != 4)) {
        throw std::invalid_argument("setup");
    }
    mode = mode == Mode::Scout ? Mode::Scout : mapMode(map);
    state_.mode = mode;
    state_.map = map;
    state_.playerCount = playerCount;
    state_.teams = teams;
    state_.teamSize = heroMode(mode) ? teamSize : 1;
    state_.turn = first; // 房主房规决定人数和地图。
    for (int i = 0; i < 8; ++i) {
        auto &u = state_.units[i];
        if (i / 2 >= playerCount || i % 2 >= state_.teamSize) {
            u.health = 0;
            continue;
        }
        if (!validRole(int(roles[i])) || !valid(mode, positions[i], map))
            throw std::invalid_argument("unit");
        u.role = heroMode(mode) ? roles[i] : Role::Sniper;
        u.position = positions[i];
        u.health = initialHealth(u.role, mode);
        u.armor = heroMode(mode) && u.role == Role::Heavy ? 1 : 0;
        if (u.role == Role::Flamer)
            u.ammo = 3;
        if (u.role == Role::WallGuard) {
            u.soldiers = formations[i] == std::array<int, 3>{} ? guardSpawn(mode, u.position, map)
                                                               : formations[i];
            if (!validFormation(mode, map, u.position, u.soldiers))
                throw std::invalid_argument("formation");
            u.soldierHealth = {1.6, 1.6, 1.6};
            u.health = 4.8;
        }
        if (i % 2 == 1 && u.role == state_.units[i - 1].role)
            throw std::invalid_argument("duplicate role");
    }
    for (int p = 0; p < 4; ++p)
        for (int i = 0; i < 8; ++i)
            resetCandidates(p, i);
    whirlpools();
    snapshots_.push_back(state_); // 未用槽位生命为零，不参与胜负。
} // 初始化结束。

void Game::resetCandidates(int observer, int unit) {
    for (int c = 1; c <= 81; ++c)
        state_.possible[observer][unit][c - 1] =
            state_.units[unit].health > 0 && valid(state_.mode, c, state_.map);
} // 保守重置不会使用秘密真实坐标缩小集合。

int Game::pullDestination(int cell) const {
    for (int c = 1; c <= cellCount(state_.mode); ++c)
        if (state_.map.cells[c - 1] == Terrain::Whirlpool && distance(state_.mode, cell, c) == 1 &&
            canMove(state_.mode, cell, c, state_.map))
            return c;
    return cell;
} // 多漩涡按格号最小者，且不连锁。

void Game::whirlpools() { // 四角色同时受到轮首效果。
    if (std::none_of(state_.map.cells.begin(), state_.map.cells.end(),
                     [](Terrain t) { return t == Terrain::Whirlpool; }))
        return; // 没有漩涡的地图不丢弃已经获得的线索。
    for (int i = 0; i < 8; ++i) {
        auto &u = state_.units[i];
        if (u.health <= 0)
            continue;
        if (u.role == Role::WallGuard) {
            bool pulled = false;
            for (int k = 0; k < 3; ++k)
                if (u.soldierHealth[k] > 0) {
                    if (u.soldierWhirl[k]) {
                        u.soldierWhirl[k] = false;
                        continue;
                    }
                    int dest = pullDestination(u.soldiers[k]);
                    if (dest != u.soldiers[k] && canEnter(view(i / 2), i, dest)) {
                        u.soldiers[k] = dest;
                        u.soldierWhirl[k] = true;
                        pulled = true;
                    }
                }
            refreshSquad(u);
            u.whirlImmune =
                std::any_of(u.soldierWhirl.begin(), u.soldierWhirl.end(), [](bool x) { return x; });
            if (pulled)
                u.report = L"士兵受到漩涡吸附，各自下一轮免疫。";
            continue;
        }
        if (u.whirlImmune) {
            u.whirlImmune = false;
            continue;
        }
        int dest = pullDestination(u.position);
        if (dest != u.position && canEnter(view(i / 2), i, dest)) {
            u.report = L"漩涡：" + coordinate(state_.mode, u.position) + L" → " +
                       coordinate(state_.mode, dest) + L"，下一轮免疫";
            u.position = dest;
            u.whirlImmune = true;
        }
    } // 士兵独立受吸附，位置同时结算且不触发附骨。

    for (int p = 0; p < 4; ++p)
        for (int i = 0; i < 8; ++i) {
            Mask next{};
            for (int c = 1; c <= cellCount(state_.mode); ++c)
                if (state_.possible[p][i][c - 1]) {
                    next[c - 1] = true;
                    next[pullDestination(c) - 1] = true;
                }
            state_.possible[p][i] = next;
        } // 对手免疫状态私密，候选合并原地与受吸引两种可能。
} // 漩涡结算结束。

int noticeWeight(const std::wstring &line) {
    if (line.find(L"投降") != std::wstring::npos || line.find(L"击杀") != std::wstring::npos ||
        line.find(L"受伤") != std::wstring::npos || line.find(L"受击") != std::wstring::npos ||
        line.find(L"破甲") != std::wstring::npos)
        return 2;
    if (line.find(L"未命中") != std::wstring::npos ||
        line.find(L"建造成功") != std::wstring::npos ||
        line.find(L"建造失败") != std::wstring::npos)
        return 1;
    return 0;
} // 战果使用大字和更长停留，不改变裁判结算。
