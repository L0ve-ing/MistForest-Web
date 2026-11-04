int inputPlayer(const View &v) {
    return v.reaction.unit >= 0 ? v.reaction.unit / 2 : v.turn;
} // 等待分担时输入交给受击方。

int inputPlayer(const Snapshot &s) {
    return s.reaction.unit >= 0 ? s.reaction.unit / 2 : s.turn;
} // 完整状态使用同样的输入归属。

bool guardWall(const Building &b) {
    return b.health && (b.kind == Structure::Wall || b.kind == Structure::GreatWall);
} // 普通城墙与长城都保留守卫军站墙加成。

int shadowTarget(Mode mode, int from, int dr, int dc) {
    int w = size(mode), h = rows(mode);
    return (((from - 1) / w + dr) % h + h) % h * w + (((from - 1) % w + dc) % w + w) % w + 1;
} // 随影首尾相连，行列分别取模。

int shadowDistance(Mode mode, int from, int to) {
    int w = size(mode), h = rows(mode), dr = std::abs((from - 1) / w - (to - 1) / w),
        dc = std::abs((from - 1) % w - (to - 1) % w);
    return std::min(dr, h - dr) + std::min(dc, w - dc);
} // 随影近战攻击使用环绕距离。

bool shadowStep(Mode mode, int from, int to, const Map &map) {
    if (!valid(mode, from, map) || !valid(mode, to, map) || shadowDistance(mode, from, to) > 1)
        return false;
    if (distance(mode, from, to) <= 1)
        return canMove(mode, from, to, map);
    int w = size(mode), h = rows(mode), r = (from - 1) / w, c = (from - 1) % w, tr = (to - 1) / w;
    if (r == tr)
        return !map.vertical[r * (w + 1)] && !map.vertical[r * (w + 1) + w];
    return !map.horizontal[c] && !map.horizontal[h * w + c];
} // 跨边界须同时没有起点和终点边墙。

bool shadowShot(Mode mode, int from, int to, const Map &map) {
    if (to < 1 || to > cellCount(mode) || map.cells[to - 1] == Terrain::Wall)
        return false;
    Map shotMap = map;
    if (shotMap.cells[to - 1] == Terrain::Lake)
        shotMap.cells[to - 1] = Terrain::Ground;
    return shadowStep(mode, from, to, shotMap);
} // 环绕射线与移动共享边墙检查，但允许攻击湖泊。

bool shadowLeap(Mode mode, int from, int dr, int dc, const Map &map) {
    if (!((std::abs(dr) == 2 && std::abs(dc) == 1) || (std::abs(dr) == 1 && std::abs(dc) == 2)))
        return false;
    int to = shadowTarget(mode, from, dr, dc), sr = std::abs(dr) == 2 ? (dr > 0 ? 1 : -1) : 0,
        sc = std::abs(dc) == 2 ? (dc > 0 ? 1 : -1) : 0, leg = shadowTarget(mode, from, sr, sc);
    if (!valid(mode, to, map) || map.cells[leg - 1] == Terrain::Wall)
        return false;
    Map edges = map;
    edges.cells.fill(Terrain::Ground);
    return shadowStep(mode, from, leg, edges);
} // 被带动或跟随日字移动时，随影仍可穿边界，保留马脚阻挡。

bool validFormation(Mode mode, const Map &map, int anchor, const std::array<int, 3> &cells) {
    return cells[0] == anchor &&
           std::all_of(cells.begin(), cells.end(), [&](int c) { return valid(mode, c, map); });
} // 延续允许同格站位的规则，连通与否只影响后续联合行动。

std::array<int, 3>
guardSpawn(Mode mode, int anchor,
           const Map &map) { // 选择起点后预览一组连通三人阵型，使用确定顺序便于联机复盘。
    std::array<int, 3> result{};
    if (!valid(mode, anchor, map))
        return result;
    result[0] = anchor;
    int count = 1;
    for (int i = 0; i < count && count < 3; ++i)
        for (int c = 1; c <= cellCount(mode) && count < 3; ++c)
            if (c != result[0] && c != result[1] && c != result[2] &&
                distance(mode, result[i], c) == 1 && canMove(mode, result[i], c, map))
                result[count++] = c;
    return result; // 不允许三名士兵初始化在同一格。
} // 自动布阵结束。

int soldierCell(const Unit &u, int piece) {
    return u.role == Role::WallGuard && piece >= 0 && piece < 3 ? u.soldiers[piece] : u.position;
} // 非守卫军仍使用原来的单单位坐标。

int connectedSquad(Mode mode, const Unit &u,
                   int piece) { // 连通关系只依据己方已知位置，不通过敌方位置判断。
    if (piece < 0 || piece > 2 || u.soldierHealth[piece] <= 0) {
        return 0;
    }
    if (u.soldierSilenced[piece])
        return 1 << piece;
    int mask = 1 << piece;
    for (int k = 0; k < 3; ++k)
        for (int i = 0; i < 3; ++i)
            if (mask & (1 << i))
                for (int j = 0; j < 3; ++j)
                    if (u.soldierHealth[j] > 0 && !u.soldierSilenced[j] &&
                        distance(mode, u.soldiers[i], u.soldiers[j]) == 1)
                        mask |= 1 << j;
    return mask; // 重叠不视为上下左右相连。
} // 小队分组结束。

void refreshSquad(Unit &u) {
    if (u.role != Role::WallGuard)
        return;
    u.health = 0;
    bool chosen = false;
    for (int k = 0; k < 3; ++k) {
        u.health += u.soldierHealth[k];
        if (u.soldierHealth[k] > 0 && !chosen) {
            u.position = u.soldiers[k];
            chosen = true;
        }
    }
} // 用第一个存活士兵作为兵种信息的锚点。

bool fortLike(const Building &b) {
    return b.kind == Structure::Fort || b.kind == Structure::GreatWall;
} // 长城独立保留类型，通行权限按堡垒。

bool canEnter(const View &v, int id, int cell) {
    if (!valid(v.mode, cell, v.map))
        return false;
    const auto &b = v.buildings[cell - 1];
    return !b.health || (fortLike(b) && b.owner == id / 2);
} // 本人堡垒和长城可进入，普通城墙及他人建筑阻挡。

bool worldMove(const View &v, int id, int from, int to) {
    return (v.units[id].role == Role::Shadow ? shadowStep(v.mode, from, to, v.map)
                                             : canMove(v.mode, from, to, v.map)) &&
           canEnter(v, id, to);
} // 建筑不改变底层地图保存格式。

bool worldShot(const View &v, int from, int to) {
    Map m = v.map;
    for (int c = 1; c <= 81; ++c)
        if (v.buildings[c - 1].health && c != from && c != to)
            m.cells[c - 1] = Terrain::Wall;
    return clearShot(v.mode, from, to, m);
} // 可以攻击建筑本体，不能穿过中途的建筑。

int shifted(Mode mode, int from, int dr, int dc) {
    int n = size(mode), r = (from - 1) / n + dr, c = (from - 1) % n + dc;
    return r < 0 || c < 0 || r >= rows(mode) || c >= n ? 0 : r * n + c + 1;
} // 带边界检查的行列位移。

std::vector<int> rayCells(const View &v, int from, int first,
                          int length) { // 直线攻击遇到第一座建筑停止，建筑格自身仍受伤。
    std::vector<int> result;
    if (distance(v.mode, from, first) != 1)
        return result;
    int n = size(v.mode), dr = (first - 1) / n - (from - 1) / n,
        dc = (first - 1) % n - (from - 1) % n, prev = from;
    for (int k = 0; k < length; ++k) {
        int to = shifted(v.mode, prev, dr, dc);
        if (!to || !worldShot(v, prev, to))
            break;
        result.push_back(to);
        if (v.buildings[to - 1].health)
            break;
        prev = to;
    }
    return result; // 固有墙与线形墙均阻挡，湖泊允许攻击穿过。
} // 直线格子生成结束。

bool buildReach(const View &v, int from, int to) {
    if (from < 1 || from > 81 || to < 1 || to > 81)
        return false;
    if (from == to)
        return true;
    Map m = v.map;
    for (int c = 1; c <= 81; ++c) {
        if (m.cells[c - 1] == Terrain::Lake)
            m.cells[c - 1] = Terrain::Ground;
        if (v.buildings[c - 1].health && c != from && c != to)
            m.cells[c - 1] = Terrain::Wall;
    }
    int n = size(v.mode), dr = (to - 1) / n - (from - 1) / n, dc = (to - 1) % n - (from - 1) % n;
    if (std::abs(dr) > 1 || std::abs(dc) > 1)
        return false;
    if (!dr || !dc)
        return canMove(v.mode, from, to, m);
    int mid1 = (from - 1) / n * n + (to - 1) % n + 1, mid2 = (to - 1) / n * n + (from - 1) % n + 1;
    return (canMove(v.mode, from, mid1, m) && canMove(v.mode, mid1, to, m)) ||
           (canMove(v.mode, from, mid2, m) && canMove(v.mode, mid2, to, m));
} // 斜角建造至少有一条两段路径可达，不能利用子弹擦墙规则隔墙造物。

bool buildSite(const View &v, int from, int to) {
    return valid(v.mode, to, v.map) && !v.buildings[to - 1].health && !v.fires[to - 1].expires &&
           !v.rubble[to - 1] && buildReach(v, from, to);
} // 公开不可建地块禁用；隐藏敌人仍只在实际建造后裁定。

bool validateLegacy(const View &v, const Action &a,
                    std::wstring *why); // 原七职业校验在主文件中定义。

bool validate(const View &v, const Action &a,
              std::wstring *why) { // 新旧职业与建筑全部经过同一个入口。
    auto fail = [&](const wchar_t *text) {
        if (why)
            *why = text;
        return false;
    };
    auto ok = [&]() {
        if (why)
            why->clear();
        return true;
    };
    if (v.reaction.unit >= 0)
        return v.winner < 0 && v.player == inputPlayer(v) && a.kind == Kind::Share &&
                       a.unit == v.reaction.unit && a.piece == v.reaction.piece &&
                       (a.subject == -1 ||
                        (a.subject >= 0 && a.subject < 3 && (v.reaction.mask & (1 << a.subject))))
                   ? ok()
                   : fail(L"请等待受击方选择伤害分担。");
    if (a.kind == Kind::Share)
        return fail(L"当前没有待分担伤害。");
    if (v.winner != -1 || v.turn != v.player || a.unit < 0 || a.unit >= 8 || a.piece < 0 ||
        a.piece > 2 || a.unit / 2 != v.player || v.units[a.unit].health <= 0)
        return fail(L"请选择自己的存活单位。");
    const auto &u = v.units[a.unit];
    const auto &b = v.budget;
    if (a.kind == Kind::Surrender)
        return ok();
    if (b.active >= 0 && b.active != a.unit)
        return fail(L"本回合不能切换角色。");
    if (pendingFollowup(v) && a.kind != (b.swordAdvance ? Kind::SwordAdvance : Kind::FlameAdvance))
        return fail(L"先完成攻击后的秘密选择，或选择留在原地。");
    if (a.kind == Kind::End)
        return ok();
    bool hero = heroMode(v.mode);
    int from = soldierCell(u, a.piece);
    if (u.role == Role::WallGuard && u.soldierHealth[a.piece] <= 0)
        return fail(L"请选择存活士兵。"); // 仅验证本人已知的信息。
    int custom = validateV102(v, a, why);
    if (custom >= 0)
        return custom != 0;
    bool guardUnit = hero && u.role == Role::WallGuard;
    bool fresh = guardUnit ? !(b.squadDone & (1 << a.piece))
                           : b.active < 0; // 每名士兵独立拥有一次行动，不受其他士兵已行动影响。
    if (a.kind == Kind::Demolish)
        return hero && u.role != Role::Wolf && fresh && a.target >= 1 &&
                       a.target <= cellCount(v.mode) && v.buildings[a.target - 1].health &&
                       distance(v.mode, from, a.target) <= 1 && buildReach(v, from, a.target)
                   ? ok()
                   : fail(
                         L"拆除占该单位一次完整行动，目标为距离1内可达建筑。"); // 守卫军拆除消耗被选士兵的行动。
    if (a.kind == Kind::BuildFort || a.kind == Kind::BuildWall) {
        if (!hero || !fresh ||
            (v.buildings[from - 1].health &&
             !(guardUnit && v.buildings[from - 1].kind == Structure::Wall)))
            return fail(L"建造必须是自己的首个动作，且不能站在建筑内。");
        if (guardUnit && a.kind == Kind::BuildFort)
            return fail(L"守卫军只能建造一格长城。");
        if (a.target < 1 || a.target > cellCount(v.mode) || a.target == from)
            return fail(L"不能在自身格建造。");
        if (a.kind == Kind::BuildFort || guardUnit)
            return distance(v.mode, from, a.target) == 1 && buildSite(v, from, a.target)
                       ? ok()
                       : fail(L"请选上下左右相邻的一格。");
        int n = size(v.mode);
        auto near = [&](int c) {
            return c >= 1 && c <= cellCount(v.mode) && c != from &&
                   std::abs((c - 1) / n - (from - 1) / n) <= 1 &&
                   std::abs((c - 1) % n - (from - 1) % n) <= 1;
        };
        return near(a.target) && near(a.extra) && distance(v.mode, a.target, a.extra) == 1 &&
                       buildSite(v, from, a.target) && buildSite(v, from, a.extra)
                   ? ok()
                   : fail(L"城墙须选周围3×3内相邻两格，不能覆盖自身。");
    } // 隐藏敌人占格由裁判处理，失败消耗该建造者行动。
    if (guardUnit) {
        if (!fresh)
            return fail(L"该士兵本回合已行动。");
        int cap = 1;
        if (a.kind == Kind::Move) {
            int n = size(v.mode), dr = (a.target - 1) / n - (from - 1) / n,
                dc = (a.target - 1) % n - (from - 1) % n;
            if (a.target < 1 || a.target > cellCount(v.mode) || (dr && dc) ||
                std::abs(dr) + std::abs(dc) > cap)
                return fail(L"所有位置移速均为1，请选择相邻目标或原地。");
            int pos = from, steps = std::abs(dr) + std::abs(dc);
            for (int step = 0; step < steps; ++step) {
                int to =
                    shifted(v.mode, pos, dr ? dr / std::abs(dr) : 0, dc ? dc / std::abs(dc) : 0);
                if (!to || !worldMove(v, a.unit, pos, to))
                    return fail(L"移动路线被阻挡。");
                pos = to;
            }
            return ok();
        }
        if (a.kind == Kind::Attack) {
            cap = guardWall(v.buildings[from - 1]) ? 2 : 1;
            return a.target >= 1 && a.target <= cellCount(v.mode) &&
                           distance(v.mode, from, a.target) <= cap && worldShot(v, from, a.target)
                       ? ok()
                       : fail(L"攻击目标不可达。");
        }
        return fail(L"请选择本士兵的移动、攻击、建造或拆除。");
    } // 士兵分别操控，移动与攻击不可由同一士兵兼用。
    bool newSkill = a.kind == Kind::Blood || a.kind == Kind::ArmorUp || a.kind == Kind::ArmorDown ||
                    a.kind == Kind::Flame || a.kind == Kind::FireStep ||
                    a.kind == Kind::FlameAdvance;
    if (newSkill && (!hero || u.silenced))
        return fail(L"沉默时不能使用技能。"); // 普通攻击和移动仍保留。
    if ((a.kind == Kind::Follow || a.kind == Kind::Lead) && a.subject >= 0 && a.subject < 8 &&
        v.units[a.subject].role == Role::WallGuard &&
        (a.extra < 0 || a.extra > 2 || v.units[a.subject].soldierHealth[a.extra] <= 0))
        return fail(L"请选择存活的敌方士兵。"); // 指定具体士兵时验证其存活和编号。
    if (a.kind == Kind::Blood)
        return u.role == Role::Alpha && !u.cooldown && b.active < 0
                   ? ok()
                   : fail(L"嗜血尚未冷却。"); // 免费技能仍锁定所选角色，并禁止随后建造。
    if (a.kind == Kind::ArmorUp)
        return u.role == Role::Armiger && !b.used && !b.attacks && !u.armorLock &&
                       u.armor < 2 - 1e-9
                   ? ok()
                   : fail(
                         L"本回合已行动、负甲达到上限或仍处于受击锁定。"); // 每次只增加一层，执行后整回合结束。
    if (a.kind == Kind::ArmorDown)
        return u.role == Role::Armiger && !b.used && !b.attacks && !b.moves && a.amount >= 0 &&
                       a.amount <= 2 && armorUnload(u.armor, a.amount) > 0
                   ? ok()
                   : fail(
                         L"请在回合开始、其他行动前选择实际拥有的卸甲层数。"); // 卸甲次数由实际护甲限制，无无限空操作。
    if (a.kind == Kind::Thrust)
        return u.role == Role::Armiger && !b.attacks && distance(v.mode, from, a.target) == 1 &&
                       !rayCells(v, from, a.target, 1 + int(std::ceil(u.armor - 1e-9))).empty()
                   ? ok()
                   : fail(L"请选择相邻方向格；刺击不能攻击自身。"); // 方向自动延伸到当前刺击距离。
    if (a.kind == Kind::Flame)
        return u.role == Role::Flamer && !b.used && a.amount >= 1 && a.amount <= u.ammo &&
                       a.amount <= 3 && distance(v.mode, from, a.target) == 1 &&
                       !rayCells(v, from, a.target, a.amount).empty()
                   ? ok()
                   : fail(
                         L"普通移动后不能喷火；弹药须足够，起点须上下左右相邻。"); // 免费火步不消耗普通行动额度。
    if (a.kind == Kind::FireStep)
        return u.role == Role::Flamer && !b.fireStep && u.ammo > 0 &&
                       (a.amount == 0 || a.amount == 1) && distance(v.mode, from, a.target) == 1 &&
                       worldMove(v, a.unit, from, a.target)
                   ? ok()
                   : fail(L"火步每回合一次，需要1枚离火及相邻可达格。"); // amount表示是否点燃终点。
    if (a.kind == Kind::FlameAdvance)
        return u.role == Role::Flamer && b.flameChoice && !b.advance &&
                       (a.target == 0 ||
                        (a.target >= 1 && a.target <= cellCount(v.mode) &&
                         v.fires[a.target - 1].expires && canEnter(v, a.unit, a.target)))
                   ? ok()
                   : fail(L"喷火后选择正在燃烧且可进入的格，或留在原地。"); // 喷火后位移最多一次。
    if (hero && (u.role == Role::Alpha || u.role == Role::Armiger || u.role == Role::Flamer)) {
        if (a.kind == Kind::Move) {
            int limit = u.role == Role::Alpha     ? 2
                        : u.role == Role::Armiger ? 2 - int(std::ceil(u.armor - 1e-9))
                                                  : 1;
            return !b.attacks && b.moves < limit && worldMove(v, a.unit, from, a.target)
                       ? ok()
                       : fail(L"移动额度用完，或已选择攻击。");
        }
        if ((a.kind == Kind::Attack || a.kind == Kind::AttackRush) && u.role == Role::Alpha)
            return !b.used && distance(v.mode, from, a.target) <= 1 &&
                           worldShot(v, from, a.target) &&
                           (a.kind != Kind::AttackRush || worldMove(v, a.unit, from, a.target))
                       ? ok()
                       : fail(L"普攻与普通移动不能同回合；突进格必须可进入。");
        return fail(L"当前职业没有该动作。");
    } // 玄甲士和祝融不提供普通攻击。
    // 日字动作统一在下方检查动态建筑马脚与强化落点。
    if (a.kind == Kind::AttackRush) {
        return fail(L"仅阿尔法可以普通攻击同时突进。");
    }
    View adjusted = v;
    bool moving = a.kind == Kind::Move || a.kind == Kind::Lead || a.kind == Kind::Dash ||
                  a.kind == Kind::Leap || a.kind == Kind::Hunt || a.kind == Kind::Retreat ||
                  a.kind == Kind::Charge || a.kind == Kind::CleaveRush;
    for (int c = 1; c <= 81; ++c)
        if (v.buildings[c - 1].health && c != u.position &&
            (moving ? !canEnter(v, a.unit, c) : c != a.target))
            adjusted.map.cells[c - 1] = Terrain::Wall;
    if (a.kind == Kind::Charge && b.used >= 1 && a.target >= 1 && a.target <= cellCount(v.mode) &&
        v.buildings[a.target - 1].health)
        adjusted.map.cells[a.target - 1] = v.map.cells[a.target - 1];
    if (u.role == Role::Shadow && (a.kind == Kind::Move || a.kind == Kind::Lead)) {
        if (!worldMove(v, a.unit, from, a.target))
            return fail(L"移动被墙体或建筑阻挡。");
        adjusted.map = v.map;
    }
    if (u.role == Role::Knight &&
        (a.kind == Kind::Leap || a.kind == Kind::Hunt || a.kind == Kind::Retreat)) {
        adjusted.map = v.map;
        for (int c = 1; c <= cellCount(v.mode); ++c) {
            auto wall = v.buildings[c - 1];
            if (wall.health && !(fortLike(wall) && wall.owner == a.unit / 2))
                adjusted.map.cells[c - 1] = Terrain::Wall;
        }
        if (!canEnter(v, a.unit, a.target))
            return fail(L"建筑阻挡落点，包括日字移动。");
    }
    return validateLegacy(adjusted, a, why); // 新建筑参与旧职业移动、射线及马脚判定。
} // 全部动作校验结束。

void Game::destroyBuilding(int cell) {
    state_.buildings[cell - 1] = {};
    state_.rubble[cell - 1] = state_.turns + state_.playerCount;
} // 自摧毁起经过两个玩家回合自动清除废墟。

void Game::checkVictory() {
    int surviving = -1;
    bool opposed = false;
    for (int p = 0; p < state_.playerCount; ++p) {
        bool alive = false;
        for (int i = p * 2; i < p * 2 + state_.teamSize; ++i)
            alive = alive || state_.units[i].health > 0;
        state_.eliminated[p] = !alive;
        if (alive) {
            if (surviving < 0)
                surviving = p;
            else if (!allied(state_.teams, surviving, p))
                opposed = true;
        }
    }
    state_.winner = opposed ? -1 : surviving < 0 ? 4 : state_.teams ? surviving % 2 : surviving;
} // 最后存活玩家或阵营获胜，四表示平局。

void Game::damage(int actor, int target, bool ranged, bool execute, Event &e, bool hitSelf,
                  double power, bool burning, int burningPlayer) {
    if (target < 1 || target > cellCount(state_.mode))
        return;
    if (execute)
        power = 1e6;
    if (!burning)
        state_.roundDamage[target - 1] = std::max(
            state_.roundDamage[target - 1], power); // 只记录公开攻击格，不能给其他对象提供免伤。
    const damage::Attack attack{power, ranged, execute, burning};
    auto reportHit = [&](const damage::Hit &hit, const std::wstring &who) {
        if (burning)
            return;
        if (hit.delta <= 1e-9)
            e.publicText += L"\n" + who + L"本次受击周期已有不低于此次的直接攻击强度，不叠加。";
        else if (hit.prior > 0 && !execute)
            e.publicText += L"\n" + who + L"本次受击周期强度提高到" + halfPoints(hit.strength) +
                            L"，仅补差额" + halfPoints(hit.delta) + L"。";
    };
    bool contact = false, alphaHit = false;
    auto &building = state_.buildings[target - 1];
    bool protectedUnits = damage::buildingProtectsUnits(
        building, execute); // 本次攻击开始时的建筑继续保护内部单位，不溢出伤害。
    if (building.health > 0 && (!burning || burningPlayer < 0 || building.owner == burningPlayer)) {
        contact = true;
        if (!burning && allied(state_.teams, building.owner, actor / 2))
            e.publicText += L"\n己方建筑免疫攻击友伤。";
        else if (state_.units[actor].role == Role::Wolf && !burning)
            e.publicText += L"\n座狼不能伤害建筑。";
        else if (execute) {
            destroyBuilding(target);
            e.publicText += L"\n绝影摧毁建筑。";
        } else {
            alphaHit = !burning;
            const auto result = damage::againstBuilding(building, state_.units[actor].role, attack);
            building.directDamage = result.hit.ledgerAfter;
            reportHit(result.hit, L"建筑");
            double delta = result.hit.delta;
            if (delta > 1e-9) {
                building.health = result.healthAfter;
                e.publicText += L"\n建筑受伤，余" + halfPoints(building.health);
                if (building.health <= 1e-9)
                    destroyBuilding(target);
            }
        }
    }
    for (auto &rose : state_.roses)
        if (rose.cell == target && rose.health > 0 &&
            (!burning || burningPlayer < 0 || rose.unit / 2 == burningPlayer) &&
            (burning || !allied(state_.teams, rose.unit / 2, actor / 2))) {
            contact = true;
            alphaHit = !burning;
            const auto result = damage::againstRose(rose, attack);
            rose.directDamage = result.hit.ledgerAfter;
            reportHit(result.hit, L"玫瑰");
            double delta = result.hit.delta;
            if (delta > 1e-9) {
                rose.health = result.healthAfter;
                e.publicText += L"\n玫瑰余" + halfPoints(rose.health);
            }
        }
    for (int j = 0; j < 8; ++j) {
        auto &victim = state_.units[j];
        if (protectedUnits || victim.health <= 0 ||
            (burning && burningPlayer >= 0 && j / 2 != burningPlayer) ||
            (burning ? victim.role == Role::Flamer
                     : (allied(state_.teams, j / 2, actor / 2) && !(hitSelf && j == actor))))
            continue;
        int pieces = victim.role == Role::WallGuard ? 3 : 1;
        bool matched = false;
        for (int k = 0; k < pieces; ++k) {
            double &hp = pieces == 3 ? victim.soldierHealth[k] : victim.health;
            if (hp <= 0 || soldierCell(victim, k) != target)
                continue;
            contact = matched = true;
            alphaHit = !burning;
            std::wstring who =
                roleName(victim.role) + (pieces == 3 ? L"·士兵" + std::to_wstring(k + 1) : L"");
            double &ledger = pieces == 3 ? victim.soldierDirectDamage[k] : victim.directDamage;
            const auto result = damage::againstUnit(victim, k, attack);
            ledger = result.hit.ledgerAfter;
            reportHit(result.hit, who);
            double delta = result.hit.delta;
            if (delta <= 1e-9)
                continue;
            if (victim.role == Role::Armiger)
                victim.armorLock = state_.turn == j / 2 ? 3 : 2;
            if (execute) {
                hp = result.healthAfter;
                victim.temporaryHealth = result.temporaryAfter;
                victim.armor = result.armorAfter;
                victim.shield = result.shieldAfter;
                appendPublicCue(e, L"\n击杀 · " + who + L"（绝影）", PublicEffect::Kill);
                continue;
            }
            if (!burning && e.action.kind == Kind::Attack && state_.units[actor].role == Role::Ink)
                state_.units[actor].cooldown = 0;
            victim.armor = result.armorAfter;
            victim.shield = result.shieldAfter;
            double absorbed = result.armorDamage, shield = result.shieldDamage;
            damage::GuardSharing sharing;
            if (!burning && pieces == 3 && result.bodyDamage > 0)
                sharing = damage::shareGuards(size(state_.mode), victim.soldiers,
                                              victim.soldierHealth, k, result.bodyDamage);
            if (sharing.shared) {
                victim.soldierHealth = sharing.healthAfter;
                e.publicText += L"\n列阵自动平摊，每人" + halfPoints(sharing.each);
            } else {
                hp = result.healthAfter;
                victim.temporaryHealth = result.temporaryAfter;
            }
            if (hp < 1e-9)
                hp = 0;
            appendPublicCue(
                e, hp > 0 ? L"\n受击 · " + who + L"，余" + halfPoints(hp) : L"\n击杀 · " + who,
                hp > 0 ? PublicEffect::Hit : PublicEffect::Kill, absorbed > 0 || shield > 0);
            if (absorbed)
                e.publicText += L"，护甲吸收" + halfPoints(absorbed);
            if (shield)
                e.publicText += L"，临甲吸收" + halfPoints(shield);
        }
        refreshSquad(victim);
        if (!burning && pieces != 3) {
            if (matched) {
                state_.possible[actor / 2][j] = {};
                state_.possible[actor / 2][j][target - 1] = victim.health > 0;
            } else
                state_.possible[actor / 2][j][target - 1] = false;
        }
        if (victim.health <= 0) {
            victim.follow = -1;
            for (auto &u : state_.units)
                if (u.follow == j)
                    u.follow = -1;
            for (int p = 0; p < 4; ++p)
                state_.possible[p][j] = {};
        }
    }
    auto &attacker = state_.units[actor];
    if (alphaHit && !burning && attacker.role == Role::Alpha && attacker.health > 0 &&
        (e.action.kind == Kind::Attack || e.action.kind == Kind::AttackRush)) {
        double baseHealth = attacker.health - attacker.temporaryHealth;
        double healed =
            std::min(1.0, std::max(0.0, initialHealth(attacker.role, state_.mode) - baseHealth));
        attacker.health += healed;
        double overflow = state_.budget.blood ? 1.0 - healed : 0.0;
        attacker.temporaryHealth += overflow;
        attacker.health += overflow;
        e.publicText += L"\n嗜血命中回复" + halfPoints(healed);
        if (overflow > 0)
            e.publicText += L"，溢出转为临时血量" + halfPoints(overflow);
    }
    if (!contact)
        e.publicText += L"\n未命中";
}

void Game::build(const Action &a, Event &e) { // 普通角色的两格城墙一次建成或一次失败。
    auto &u = state_.units[a.unit];
    std::vector<int> cells{a.target};
    if (a.kind == Kind::BuildWall)
        cells.push_back(a.extra);
    bool allowed = true;
    for (int c : cells) {
        if (!buildSite(view(state_.turn), u.position, c))
            allowed = false;
        for (const auto &other : state_.units)
            if (other.health > 0) {
                if (other.role == Role::WallGuard) {
                    for (int k = 0; k < 3; ++k)
                        if (other.soldierHealth[k] > 0 && other.soldiers[k] == c)
                            allowed = false;
                } else if (other.position == c)
                    allowed = false;
            }
    }
    e.publicText += a.kind == Kind::BuildFort ? L"建造堡垒" : L"建造城墙";
    for (int c : cells)
        e.publicText += coordinate(state_.mode, c);
    if (!allowed) {
        e.publicText += L"\n建造失败，回合结束。";
        return;
    }
    for (int c : cells)
        state_.buildings[c - 1] = {a.kind == Kind::BuildFort ? Structure::Fort : Structure::Wall,
                                   a.unit / 2, 2};
    e.publicText += L"\n建造成功，回合结束。";
    u.report = L"建筑位置已向双方公开。"; // 失败不说明是隐藏敌人还是其他冲突。
} // 建造结束。

void Game::ignite(
    int actor, int cell, Event &e, bool immediate,
    bool hitSelf) { // 火焰持续至施放者下一回合开始；燃烧按受影响对象所属玩家的回合末结算。
    if (state_.map.cells[cell - 1] == Terrain::Lake ||
        state_.map.cells[cell - 1] == Terrain::Wall) {
        return;
    }
    bool building = state_.buildings[cell - 1].health > 0;
    if (building && immediate)
        damage(actor, cell, true, false, e, hitSelf);
    state_.fires[cell - 1] = {actor / 2, state_.playerTurns[actor / 2] + 1};
    if (immediate && !building)
        damage(actor, cell, true, false, e,
               hitSelf); // 直接火焰可伤及施放者和敌方祝融；只免疫持续燃烧，另行结算。
} // 点燃结束。

void Game::guardAction(const Action &a, Event &e) { // 每次只执行选中士兵的一次行动。
    auto &u = state_.units[a.unit];
    auto &b = state_.budget;
    int from = u.soldiers[a.piece], n = size(state_.mode);
    b.squadMask = 0;
    for (int k = 0; k < 3; ++k)
        if (u.soldierHealth[k] > 0)
            b.squadMask |= 1 << k;
    b.squadDone |= 1 << a.piece;
    ++b.used; // 全队存活者可任意顺序行动。
    if (a.kind == Kind::Attack) {
        e.attackCells[a.target - 1] = true;
        ++b.attacks;
        e.publicText += L"士兵" + std::to_wstring(a.piece + 1) +
                        (guardWall(state_.buildings[from - 1]) ? L"抛射" : L"攻击") +
                        coordinate(state_.mode, a.target);
        b.hitCells[a.target - 1] = true;
        damage(a.unit, a.target, guardWall(state_.buildings[from - 1]), false, e);
        return;
    } // 直接受击士兵各自记录强度，分担保持原规则。
    if (a.kind == Kind::BuildWall) {
        int c = a.target;
        bool allowed = buildSite(view(state_.turn), from, c) &&
                       (!state_.buildings[from - 1].health ||
                        state_.buildings[from - 1].kind == Structure::Wall);
        for (const auto &other : state_.units)
            if (other.health > 0) {
                if (other.role == Role::WallGuard) {
                    for (int j = 0; j < 3; ++j)
                        if (other.soldierHealth[j] > 0 && other.soldiers[j] == c)
                            allowed = false;
                } else if (other.position == c)
                    allowed = false;
            }
        e.publicText += L"士兵" + std::to_wstring(a.piece + 1) + L"建造长城" +
                        coordinate(state_.mode, c) + (allowed ? L"成功" : L"失败") +
                        L"，该士兵行动结束。";
        if (allowed)
            state_.buildings[c - 1] = {Structure::GreatWall, a.unit / 2, 2};
        return;
    } // 每人最多一格长城，失败不阻止其他士兵行动。
    if (a.kind == Kind::Demolish) {
        destroyBuilding(a.target);
        e.publicText += L"士兵" + std::to_wstring(a.piece + 1) + L"拆除建筑" +
                        coordinate(state_.mode, a.target);
        return;
    } // 拆除同样只消耗本人行动。
    int dr = (a.target - 1) / n - (from - 1) / n, dc = (a.target - 1) % n - (from - 1) % n,
        steps = std::abs(dr) + std::abs(dc);
    dr = dr ? dr / std::abs(dr) : 0;
    dc = dc ? dc / std::abs(dc) : 0;
    for (int step = 0; step < steps; ++step) {
        int to = shifted(state_.mode, u.soldiers[a.piece], dr, dc);
        u.soldiers[a.piece] = to;
        for (int j = 0; j < 8; ++j) {
            auto &f = state_.units[j];
            if (f.health <= 0 || (f.role == Role::Shadow && f.silenced) || f.follow != a.unit ||
                f.followPiece != a.piece || immune(f))
                continue;
            int dest = f.role == Role::Shadow ? shadowTarget(state_.mode, f.position, dr, dc)
                                              : shifted(state_.mode, f.position, dr, dc);
            bool allowed = dest && worldMove(view(j / 2), j, f.position, dest);
            if (allowed)
                f.position = dest;
            f.report += allowed ? L"\n附骨同步到" + coordinate(state_.mode, dest)
                                : L"\n附骨跟随士兵受阻，跳过这一步。";
            for (int p = 0; p < 4; ++p)
                resetCandidates(p, j);
        }
    }
    refreshSquad(u);
    for (int p = 0; p < 4; ++p)
        resetCandidates(p, a.unit);
    ++b.moves;
    e.publicText += L"士兵" + std::to_wstring(a.piece + 1) + L"移动";
    e.privateText = L"从" + coordinate(state_.mode, from) + L"到" +
                    coordinate(state_.mode, u.soldiers[a.piece]); // 移动方向、终点仍只向本人提供。
} // 独立士兵动作结束。
