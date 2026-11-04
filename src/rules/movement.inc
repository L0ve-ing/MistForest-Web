bool pendingFollowup(const View &v) {
    return v.budget.swordAdvance || v.budget.flameChoice;
}

int inkPelletLimit(const Turn &) {
    return 3;
} // 白墨每回合三发，冲刺不增加弹药。

bool silenceArea(Mode mode, int origin, int target, const Map &) {
    return origin >= 1 && target >= 1 && distance(mode, origin, target) <= 1;
} // 界面范围与实际沉默使用相同判定，不包含斜角与距离二格。

bool immune(const Unit &u) {
    return u.role == Role::HiddenWolf && u.form == 0;
} // 仅人形隐狼免疫探测及附骨。

int attackRange(Mode mode, const Unit &u) {
    return mode == Mode::Scout                         ? 1
           : u.role == Role::Sniper                    ? 3
           : u.role == Role::HiddenWolf && u.form == 0 ? 2
                                                       : 1;
} // 射程与伤害类型独立。

std::vector<int> dashPath(Mode mode, int from, int to, const Map &map) {
    if (!valid(mode, from, map) || to < 1 || to > cellCount(mode))
        return {};
    int w = size(mode), dr = (to - 1) / w - (from - 1) / w,
        dc = (to - 1) % w - (from - 1) % w;
    if ((dr != 0 && dc != 0) || std::abs(dr) + std::abs(dc) < 1 ||
        std::abs(dr) + std::abs(dc) > 2)
        return {};
    dr = (dr > 0) - (dr < 0);
    dc = (dc > 0) - (dc < 0);
    std::vector<int> path;
    int previous = from;
    for (int step = 0; step < 2; ++step) {
        int r = (previous - 1) / w + dr, c = (previous - 1) % w + dc;
        if (r < 0 || r >= rows(mode) || c < 0 || c >= w)
            break;
        int next = r * w + c + 1;
        if (!canMove(mode, previous, next, map))
            break;
        path.push_back(next);
        previous = next;
    }
    return path;
} // 选择直线方向，最多走两格；墙、建筑、湖泊及边界均会提前阻挡。

std::vector<int> dashPath(const View &v, int unit, int target) {
    Map map = v.map;
    for (int cell = 1; cell <= cellCount(v.mode); ++cell)
        if (!canEnter(v, unit, cell))
            map.cells[cell - 1] = Terrain::Wall;
    return dashPath(v.mode, v.units[unit].position, target, map);
}

bool canLeap(Mode mode, int from, int to, const Map &map) { // 日字落点固定，跨越途中其他墙不阻挡。
    if (!valid(mode, from, map) || !valid(mode, to, map)) {
        return false;
    }
    int n = size(mode), r = (from - 1) / n, c = (from - 1) % n, dr = (to - 1) / n - r,
        dc = (to - 1) % n - c; // 起点和终点必须能站人。
    if (!((std::abs(dr) == 2 && std::abs(dc) == 1) || (std::abs(dc) == 2 && std::abs(dr) == 1)))
        return false; // 仅允许八个日字方向。
    int sr = std::abs(dr) == 2 ? (dr > 0 ? 1 : -1) : 0,
        sc = std::abs(dc) == 2 ? (dc > 0 ? 1 : -1) : 0,
        mid = (r + sr) * n + c + sc + 1; // 第一格为马脚，第二格是转向点。
    Map edgeMap = map;
    edgeMap.cells.fill(Terrain::Ground); // 跳跃中间不需落地，湖泊可跨越；只检查起点到第一格的边墙。
    return map.cells[mid - 1] != Terrain::Wall &&
           canMove(mode, from, mid, edgeMap); // 方墙挡马脚，仅起点到马脚的线墙阻挡。
} // 日字检测结束。
