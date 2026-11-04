void Game::moveUnit(int id, int dest, bool leap, bool publicPosition, int lead, int leadPiece,
                    bool blink) {
    if (leap) {
        copyLeap(id, dest);
        state_.units[id].position = dest;
        for (int p = 0; p < 4; ++p)
            resetCandidates(p, id);
        if (publicPosition)
            for (int p = 0; p < 4; ++p) {
                state_.possible[p][id] = {};
                state_.possible[p][id][dest - 1] = true;
            }
        return;
    } // 同步复制在伤害之前完成。
    auto &actor = state_.units[id];
    int w = size(state_.mode), h = rows(state_.mode), from = actor.position,
        dr = (dest - 1) / w - (from - 1) / w, dc = (dest - 1) % w - (from - 1) % w;
    if (actor.role == Role::Shadow) {
        if (std::abs(dr) > h / 2)
            dr += dr > 0 ? -h : h;
        if (std::abs(dc) > w / 2)
            dc += dc > 0 ? -w : w;
    } // 穿边界复制实际一步方向，不复制整幅地图位移。

    struct Pending {
        int id, piece, to;
    };

    std::vector<Pending> pending;
    for (int j = 0; j < 8; ++j) {
        auto &f = state_.units[j];
        if (j == id || f.health <= 0 || (f.role == Role::Shadow && f.silenced) || immune(f) ||
            immune(actor) || (f.follow != id && j != lead))
            continue;
        int piece = f.role == Role::WallGuard ? leadPiece : 0;
        if (piece < 0 || piece > 2 || (f.role == Role::WallGuard && f.soldierHealth[piece] <= 0))
            continue;
        int pos = soldierCell(f, piece), to = f.role == Role::Shadow
                                                  ? shadowTarget(state_.mode, pos, dr, dc)
                                                  : shifted(state_.mode, pos, dr, dc);
        auto v = view(j / 2);
        Map m = state_.map;
        for (int c = 1; c <= cellCount(state_.mode); ++c)
            if (!canEnter(v, j, c))
                m.cells[c - 1] = Terrain::Wall;
        bool allowed =
            to && (leap    ? (canEnter(v, j, to) &&
                              (f.role == Role::Shadow ? shadowLeap(state_.mode, pos, dr, dc, m)
                                                      : canLeap(state_.mode, pos, to, m)))
                   : blink ? canEnter(v, j, to)
                           : worldMove(v, j, pos, to));
        if (allowed)
            pending.push_back({j, piece, to});
        f.report +=
            allowed ? L"\n附骨同步到" + coordinate(state_.mode, to) : L"\n附骨受阻，跳过这一步。";
        if (j == lead)
            actor.report += allowed ? L"\n带动反馈：目标未受阻。" : L"\n带动反馈：目标受到阻挡。";
    } // 向带动者报告是否阻挡，不报告目标位置。
    actor.position = dest;
    for (int p = 0; p < 4; ++p)
        resetCandidates(p, id);
    for (auto item : pending) {
        auto &u = state_.units[item.id];
        if (u.role == Role::WallGuard) {
            u.soldiers[item.piece] = item.to;
            refreshSquad(u);
        } else
            u.position = item.to;
        for (int p = 0; p < 4; ++p)
            resetCandidates(p, item.id);
    }
    if (publicPosition)
        for (int p = 0; p < 4; ++p) {
            state_.possible[p][id] = {};
            state_.possible[p][id][dest - 1] = true;
        } // 不共享队友秘密站位。
} // 同步位移结束。

void Game::revealCharge(Event &e) {
    if (state_.budget.chargeCells.empty())
        return;
    auto cells = state_.budget.chargeCells;
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
    appendPublicCue(e, L"\n冲锋判定格：", PublicEffect::Charge);
    for (int c : cells) {
        e.publicText += coordinate(state_.mode, c) + L" ";
        e.attackCells[c - 1] = true;
    }
} // 排序不标明方向和终点，不逐格播报判定坐标。

void Game::endTurn(Event &e) { // 每玩家独立冷却，所有存活玩家各行动一次构成一轮。
    revealCharge(e);
    int player = state_.turn;
    ++state_.playerTurns[player];
    for (int i = 0; i < 8; ++i) {
        auto &u = state_.units[i];
        if (i / 2 == player) {
            u.cooldown = std::max(0, u.cooldown - 1);
            u.armorLock = std::max(0, u.armorLock - 1);
            if (u.role == Role::Flamer && u.health > 0) {
                if (u.ammoBlocked)
                    e.playerNotes[player] += L"火步点燃：本回合不回复离火。";
                else
                    u.ammo = std::min(3, u.ammo + (state_.fires[u.position - 1].expires ? 2 : 1));
                u.ammoBlocked = false;
            }
            if (u.role == Role::Sword && u.health > 0)
                u.swordQi = std::min(swordCap(state_.swords, i), u.swordQi + 3);
            for (auto &cd : u.optionCooldown)
                cd = std::max(0, cd - 1);
            if (u.role != Role::Shadow) {
                if (u.controlSource >= 0) {
                    // 游猎控制在施放者下一回合开始解除。
                } else if (u.silenced) {
                    u.silenced = false;
                    u.guard = true;
                } else
                    u.guard = false;
            }
            for (int k = 0; k < 3; ++k) {
                if (u.soldierControlSource[k] >= 0) {
                    // 士兵分别保留施放者计时。
                } else if (u.soldierSilenced[k]) {
                    u.soldierSilenced[k] = false;
                    u.soldierGuard[k] = true;
                } else
                    u.soldierGuard[k] = false;
            }
        } else {
            if (u.follow >= 0 && u.follow / 2 == player)
                u.follow = -1;
        }
    } // 跟随仅在指定目标所属玩家回合后清除。
    for (int c = 1; c <= cellCount(state_.mode); ++c) {
        if (state_.fires[c - 1].expires) {
            e.publicText += L"\n燃烧结算" + coordinate(state_.mode, c);
            damage(state_.fires[c - 1].owner * 2, c, true, false, e, false, 2, true, player);
        }
    }
    ++state_.turns;
    for (auto &until : state_.rubble)
        if (until && until <= state_.turns)
            until = 0;
    checkVictory();
    state_.budget = Turn{};
    state_.roundMask |= 1 << player;
    int aliveMask = 0;
    for (int p = 0; p < state_.playerCount; ++p)
        if (!state_.eliminated[p])
            aliveMask |= 1 << p;
    if (state_.winner < 0 && state_.turns >= 40 * state_.playerCount)
        state_.winner = 4;
    if (state_.winner < 0) {
        do {
            state_.turn = (state_.turn + 1) % state_.playerCount;
        } while (state_.eliminated[state_.turn]);
        startTurn();
        if ((state_.roundMask & aliveMask) == aliveMask) {
            state_.roundMask = 0;
            state_.roundDamage = {}; // 公共攻击格标记仍按整轮清空，不影响对象的受击周期。
            ++state_.roundNumber;
            whirlpools();
        }
    } // 淘汰玩家跳过，最多四十轮。
} // 回合推进结束。

bool Game::actCore(const Action &a) { // 每次只执行一个已经校验的动作，方便玩家观察结果后决策。
    if (!validate(view(state_.turn), a)) {
        return false;
    }
    if (a.kind == Kind::Surrender) {
        Event e;
        e.turn = state_.turns;
        e.player = state_.turn;
        e.unit = a.unit;
        e.action = a;
        e.publicText = L"玩家" + std::to_wstring(state_.turn + 1) + L"投降。";
        for (int i = state_.turn * 2; i < state_.turn * 2 + 2; ++i) {
            state_.units[i].health = 0;
            state_.units[i].soldierHealth = {};
        }
        checkVictory();
        if (state_.winner < 0)
            endTurn(e);
        ++state_.revision;
        events_.push_back(e);
        snapshots_.push_back(state_);
        return true;
    }
    auto &b = state_.budget;
    auto &u = state_.units[a.unit];
    if (b.active < 0)
        b.active = a.unit;
    std::array<std::wstring, 8> beforeReports;
    for (int i = 0; i < 8; ++i)
        beforeReports[i] = state_.units[i].report;
    state_.intel = {};
    Event e;
    e.turn = state_.turns;
    e.player = state_.turn;
    e.unit = a.unit;
    e.action = a;
    e.publicText = roleName(u.role) + L" · ";
    bool end = a.kind == Kind::End; // 第一项实际操作锁定出场角色。
    auto strike = [&](int target, bool ranged, bool execute) {
        if (a.kind != Kind::Charge)
            e.attackCells[target - 1] = true;
        damage(a.unit, target, ranged, execute, e);
    }; // 伤害按当前子动作位置判断。
    if (end)
        e.publicText += L"结束回合"; // 即使只等待也能结束自己的回合。
    else if (applyV102(a, e)) {
    } else if (u.role == Role::WallGuard) {
        guardAction(a, e);
        end = false;
    } // 三名士兵分别用完行动后自动结束，已阵亡者不占预算。
    else if (a.kind == Kind::Demolish) {
        destroyBuilding(a.target);
        e.publicText += L"拆除建筑" + coordinate(state_.mode, a.target) + L"，留下废墟。";
        end = true;
    } // 普通角色拆除仍占整回合。
    else if (a.kind == Kind::BuildFort || a.kind == Kind::BuildWall) {
        build(a, e);
        end = true;
    } // 普通角色建造规则不变。
    else if (a.kind == Kind::Blood) {
        b.blood = true;
        u.cooldown = 3;
        for (int p = 0; p < state_.playerCount; ++p)
            if (!allied(state_.teams, p, state_.turn))
                state_.intel[p][u.position - 1] = true;
        e.publicText += L"嗜血：自身瞬间位置" + coordinate(state_.mode, u.position);
        int nearest = 99, cell = 0;
        for (int j = 0; j < 8; ++j)
            if (!allied(state_.teams, j / 2, a.unit / 2) && state_.units[j].health > 0 &&
                !immune(state_.units[j])) {
                const auto &target = state_.units[j];
                for (int k = 0; k < (target.role == Role::WallGuard ? 3 : 1); ++k) {
                    if (target.role == Role::WallGuard && target.soldierHealth[k] <= 0)
                        continue;
                    int c = soldierCell(target, k), d = distance(state_.mode, u.position, c);
                    if (d < nearest || (d == nearest && c < cell)) {
                        nearest = d;
                        cell = c;
                    }
                }
            }
        e.privateText =
            cell ? L"最近敌方单位瞬间位置" + coordinate(state_.mode, cell) + L"（不提供职业）"
                 : L"没有可探测敌方单位。";
        if (cell) {
            for (int j = 0; j < 8; ++j)
                if (!allied(state_.teams, j / 2, state_.turn) && state_.units[j].health > 0 &&
                    !immune(state_.units[j]))
                    for (int c = 1; c <= cellCount(state_.mode); ++c)
                        if (distance(state_.mode, u.position, c) < nearest)
                            state_.possible[state_.turn][j][c - 1] = false;
        }
        u.report = e.privateText;
        e.feedback = L"嗜血 · " + u.report;
        if (cell)
            state_.intel[state_.turn][cell - 1] = true;
    } // 同距离按坐标确定一个目标，标记不追踪后续位置。
    else if (a.kind == Kind::ArmorUp) {
        u.armor = std::min(2.0, std::ceil(u.armor + 1e-9));
        e.publicText += L"甲兵·负甲，现有" + std::to_wstring(u.armor) + L"层";
        end = true;
    } // 负甲消耗整个回合。
    else if (a.kind == Kind::ArmorDown) {
        double amount = armorUnload(u.armor, a.amount);
        u.armor -= amount;
        e.publicText += L"甲兵·卸甲" + halfPoints(amount * 2) + L"层";
    } // 卸甲也属于行动，因此不能随后建造。
    else if (a.kind == Kind::Thrust) {
        auto cells =
            rayCells(view(state_.turn), u.position, a.target, 1 + int(std::ceil(u.armor - 1e-9)));
        ++b.used;
        ++b.attacks;
        e.publicText += L"甲兵·直线刺击";
        for (int cell : cells) {
            e.publicText += L"\n判定" + coordinate(state_.mode, cell);
            strike(cell, false, false);
        }
    } // 每个沿线格子伤害一次，建筑阻断后续格子。
    else if (a.kind == Kind::Flame) {
        auto cells = rayCells(view(state_.turn), u.position, a.target, a.amount);
        u.ammo -= a.amount;
        ++b.used;
        ++b.attacks;
        b.flameChoice = !b.advance;
        e.publicText += L"火舞·喷射，消耗" + std::to_wstring(a.amount) + L"离火";
        for (int cell : cells) {
            e.publicText += L"\n火焰" + coordinate(state_.mode, cell);
            e.attackCells[cell - 1] = true;
            ignite(a.unit, cell, e, true);
            if (state_.map.cells[cell - 1] != Terrain::Lake)
                b.flameCells[cell - 1] = true;
        }
    } // 即时火伤和建筑焚毁采用统一规则。
    else if (a.kind == Kind::FireStep) {
        --u.ammo;
        b.fireStep = true;
        moveUnit(a.unit, a.target, false, false);
        e.publicText +=
            a.amount ? L"火舞·火步并点燃" + coordinate(state_.mode, a.target) : L"火舞·火步";
        e.privateText = L"火步到" + coordinate(state_.mode, a.target);
        if (a.amount) {
            e.attackCells[a.target - 1] = true;
            u.ammoBlocked = true;
            ignite(a.unit, a.target, e, true, false);
            e.privateText += L"；本回合结束不回复离火。";
        }
    } // 火步点燃仅直接伤害终点敌人，本回合取消全部回复离火；纯移动不受此限制。
    else if (a.kind == Kind::FlameAdvance) {
        b.flameChoice = false;
        if (a.target)
            b.advance = true;
        if (a.target)
            moveUnit(a.unit, a.target, false, false, -1, 0, true);
        e.publicText += L"攻击后选择完成";
        e.privateText = a.target ? L"进入火焰" + coordinate(state_.mode, a.target) : L"留在原地。";
    } // 进入任一正在燃烧的格，每回合一次。
    else if (a.kind == Kind::Transform) {
        u.form = a.target;
        u.cooldown = 0;
        b.transformed = true;
        if (!u.form) {
            u.shield = std::max(u.shield, 1.0);
        }
        e.publicText += u.form ? L"变为狼形" : L"变为人形，获得0.5临时护甲";
    } // 变身不消耗普通行动，但之后不可再变。
    else if (a.kind == Kind::Follow) {
        u.follow = a.subject;
        u.followPiece = a.extra;
        b.followed = true;
        e.publicText += L"附骨指定" + roleName(state_.units[a.subject].role);
    } // 指定并限制本回合移动预算。
    else if (a.kind == Kind::Move || a.kind == Kind::Lead) {
        if (a.kind == Kind::Lead)
            b.led = true;
        moveUnit(a.unit, a.target, false, false, a.kind == Kind::Lead ? a.subject : -1, a.extra);
        if (u.role == Role::HiddenWolf && u.form && b.attacks)
            b.instinct = false;
        else
            ++b.moves;
        ++b.used;
        if (u.role == Role::Heavy)
            b.phase = 1;
        e.publicText +=
            a.kind == Kind::Lead ? L"附骨带动" + roleName(state_.units[a.subject].role) : L"移动";
        e.privateText = L"移动到" + coordinate(state_.mode, u.position);
    } // 不公布移动终点。
    else if (a.kind == Kind::Dash) {
        auto movementMap = state_.map;
        auto own = view(state_.turn);
        for (int c = 1; c <= 81; ++c)
            if (!canEnter(own, a.unit, c))
                movementMap.cells[c - 1] = Terrain::Wall;
        auto path = dashPath(state_.mode, u.position, a.target, movementMap);
        b.dashLast = false;
        b.dashed = true;
        u.cooldown = 3;
        for (int cell : path)
            moveUnit(a.unit, cell, false, false);
        ++b.used;
        e.publicText += L"炫空斗舞";
        e.privateText = L"冲刺到" + coordinate(state_.mode, u.position);
    } // 两个普通子步分别同步附骨。
    else if (a.kind == Kind::Charge) {
        int from = u.position;
        b.dr = (a.target - 1) / size(state_.mode) - (from - 1) / size(state_.mode);
        b.dc = (a.target - 1) % size(state_.mode) - (from - 1) % size(state_.mode);
        b.phase = 2;
        ++b.used;
        bool friendlyFort = state_.buildings[a.target - 1].health &&
                            fortLike(state_.buildings[a.target - 1]) &&
                            state_.buildings[a.target - 1].owner == a.unit / 2;
        bool blocked = state_.buildings[a.target - 1].health > 0 && !friendlyFort;
        if (!blocked)
            moveUnit(a.unit, a.target, false, false);
        else
            end = true;
        e.publicText += blocked ? L"冲锋撞毁建筑并占据原格" : L"冲锋推进";
        e.privateText =
            L"第" + std::to_wstring(b.used) + L"步，停留" + coordinate(state_.mode, u.position);
        if (b.used >= 2) {
            b.chargeCells.push_back(a.target);
            strike(a.target, false, false);
            if (blocked && !state_.buildings[a.target - 1].health)
                moveUnit(a.unit, a.target, false, false);
        }
        e.privateText =
            L"第" + std::to_wstring(b.used) + L"步，停留" + coordinate(state_.mode, u.position);
        if (b.used == 4)
            end = true;
    } // 四步自动结束，之前可主动停下。
    else if (a.kind == Kind::Attack && u.role == Role::Ink) {
        ++b.attacks;
        ++b.used;
        ++b.pellets;
        e.publicText += L"炫空斗舞·霰弹" + std::to_wstring(b.pellets) + L"/" +
                        std::to_wstring(inkPelletLimit(b)) + L" " +
                        coordinate(state_.mode, a.target);
        e.attackCells[a.target - 1] = true;
        damage(a.unit, a.target, true, false, e, false, 1);
    } // 每发半点伤害，移动和冲刺可插在任意两发之间。
    else if (a.kind == Kind::Attack || a.kind == Kind::AttackRush) {
        ++b.attacks;
        ++b.used;
        e.publicText += skillName(u.role) + L"攻击" + coordinate(state_.mode, a.target);
        if (a.kind == Kind::AttackRush)
            moveUnit(a.unit, a.target, false, false);
        strike(a.target,
               u.role == Role::Sniper || u.role == Role::Ink ||
                   (u.role == Role::HiddenWolf && !u.form),
               heroMode(state_.mode) && u.role == Role::Sniper);
        if (u.role == Role::HiddenWolf && u.form) {
            int nearest = 99;
            for (int j = 0; j < 8; ++j)
                if (!allied(state_.teams, j / 2, a.unit / 2) && state_.units[j].health > 0 &&
                    !immune(state_.units[j])) {
                    const auto &enemy = state_.units[j];
                    for (int k = 0; k < (enemy.role == Role::WallGuard ? 3 : 1); ++k)
                        if (enemy.role != Role::WallGuard || enemy.soldierHealth[k] > 0)
                            nearest = std::min(
                                nearest, distance(state_.mode, a.target, soldierCell(enemy, k)));
                }
            u.sense = nearest == 99 ? -1 : nearest;
            if (nearest != 99)
                for (int j = 0; j < 8; ++j)
                    if (!allied(state_.teams, j / 2, state_.turn) && state_.units[j].health > 0 &&
                        !immune(state_.units[j]))
                        for (int c = 1; c <= cellCount(state_.mode); ++c)
                            if (distance(state_.mode, a.target, c) < nearest)
                                state_.possible[state_.turn][j][c - 1] = false;
            b.instinct = state_.winner < 0;
            u.report = L"本能：以攻击格" + coordinate(state_.mode, a.target) + L"为中心，" +
                       (u.sense < 0 ? std::wstring(L"无可探测敌人。")
                                    : L"最近敌人距离为" + std::to_wstring(u.sense) + L"。");
            e.feedback = u.report;
        }
    } // 距离只在攻击后计算并私下提供。
    else if (a.kind == Kind::Leap || a.kind == Kind::Hunt || a.kind == Kind::Retreat) {
        bool hunt = a.kind == Kind::Hunt;
        moveUnit(a.unit, a.target, true, hunt);
        ++b.used;
        e.publicText += hunt                      ? L"倾轧落点" + coordinate(state_.mode, a.target)
                        : a.kind == Kind::Retreat ? L"游猎秘密跳跃"
                                                  : L"基础日字移动";
        e.privateText = L"落点" + coordinate(state_.mode, a.target);
        if (hunt) {
            b.phase = 3;
            b.anchor = a.target;
            strike(a.target, false, false);
        } else if (a.kind == Kind::Retreat) {
            b.lastOption = 3;
            ++b.options;
            u.optionCooldown[2] = 0; // 第三项没有跨回合冷却。
        } else
            end = true;
    } // 基础跳跃独占回合，强化后可以依次选择后续项目。
    else if (a.kind == Kind::Silence) {
        b.lastOption = 1;
        ++b.options;
        u.optionCooldown[0] = 2;
        bool sensed = false;
        for (int j = 0; j < 8; ++j) {
            auto &target = state_.units[j];
            if (allied(state_.teams, j / 2, a.unit / 2) || target.health <= 0)
                continue;
            if (target.role == Role::WallGuard) {
                for (int k = 0; k < 3; ++k)
                    if (target.soldierHealth[k] > 0 && !target.soldierGuard[k] &&
                        distance(state_.mode, b.anchor, target.soldiers[k]) <= 1) {
                        target.soldierSilenced[k] = target.soldierVulnerable[k] = true;
                        target.soldierControlSource[k] = state_.turn;
                        target.soldierControlExpires[k] = state_.playerTurns[state_.turn] + 1;
                        sensed = true;
                    }
                continue;
            }
            if (target.guard || distance(state_.mode, b.anchor, target.position) > 1)
                continue;
            target.silenced = target.vulnerable = true;
            target.controlSource = state_.turn;
            target.controlExpires = state_.playerTurns[state_.turn] + 1;
            target.report =
                target.role == Role::Shadow
                    ? L"受到沉默：附骨暂停，持续到所属玩家下一回合开始。"
                    : L"受到沉默与易伤：技能禁用，受到伤害增加0.5，持续至施放者下一回合开始。";
            if (!immune(target))
                sensed = true;
        }
        if (sensed) {
            int sole = -1, count = 0;
            for (int j = 0; j < 8; ++j)
                if (!allied(state_.teams, j / 2, state_.turn) && state_.units[j].health > 0 &&
                    !immune(state_.units[j])) {
                    sole = j;
                    ++count;
                }
            if (count == 1 && state_.units[sole].role != Role::WallGuard)
                for (int c = 1; c <= cellCount(state_.mode); ++c)
                    if (distance(state_.mode, b.anchor, c) > 1)
                        state_.possible[state_.turn][sole][c - 1] = false;
        }
        u.report = sensed ? L"沉默：有敌方单位被控。" : L"沉默：未获得控制成功反馈。";
        e.feedback = L"游猎 · " + u.report;
        e.publicText += L"游猎①沉默与易伤（落点及距离1格）";
    } // 范围外的同队士兵不受到沉默，人形隐狼仍隐藏控制反馈。

    else if (a.kind == Kind::Cleave || a.kind == Kind::CleaveRush) {
        b.lastOption = 2;
        ++b.options;
        u.optionCooldown[1] = 2;
        e.publicText += L"游猎②挥劈" + coordinate(state_.mode, a.target);
        strike(a.target, false, false);
        if (a.kind == Kind::CleaveRush && state_.winner < 0)
            moveUnit(a.unit, a.target, false, true);
    } // 挥劈先造成伤害，再按选择突进。
    else {
        ++b.used;
        bool detected = false;
        for (int j = 0; j < 8; ++j)
            if (!allied(state_.teams, j / 2, a.unit / 2) && state_.units[j].health > 0 &&
                !immune(state_.units[j])) {
                int region = a.kind == Kind::Row
                                 ? (state_.units[j].position - 1) / size(state_.mode)
                                 : (state_.units[j].position - 1) % size(state_.mode);
                detected = detected || region == a.target;
            }
        int sole = -1, count = 0;
        for (int j = 0; j < 8; ++j)
            if (!allied(state_.teams, j / 2, state_.turn) && state_.units[j].health > 0 &&
                !immune(state_.units[j])) {
                sole = j;
                ++count;
            }
        for (int j = 0; j < 8; ++j)
            if (!allied(state_.teams, j / 2, state_.turn) && state_.units[j].health > 0 &&
                !immune(state_.units[j]))
                for (int c = 1; c <= cellCount(state_.mode); ++c) {
                    bool region = (a.kind == Kind::Row ? (c - 1) / size(state_.mode)
                                                       : (c - 1) % size(state_.mode)) == a.target;
                    if ((!detected && region) || (detected && count == 1 && j == sole && !region))
                        state_.possible[state_.turn][j][c - 1] = false;
                }
        u.report = L"侦察结果：" + std::wstring(detected ? L"有敌人" : L"无敌人");
        e.publicText +=
            L"侦察第" + std::to_wstring(a.target + 1) + (a.kind == Kind::Row ? L"行" : L"列");
    } // 仅三格模式使用旧侦察。
    if (a.kind == Kind::SwordAdvance || a.kind == Kind::FlameAdvance) {
        // Entering and staying expose the same candidate masks and notice shape.
        for (int p = 0; p < state_.playerCount; ++p)
            for (int id = 0; id < 8; ++id)
                if (id == a.unit || state_.units[id].follow == a.unit)
                    resetCandidates(p, id);
    }
    checkVictory();
    if (state_.winner >= 0) {
        end = true;
        revealCharge(e);
    }
    if (!end && !hasAction(view(state_.turn), a.unit)) {
        end = true;
        e.publicText += L"\n已无可用行动，自动结束回合。";
    }
    if (end && state_.winner < 0)
        endTurn(e);
    for (int i = 0; i < 8; ++i)
        if (state_.units[i].report != beforeReports[i] && !state_.units[i].report.empty() &&
            state_.units[i].report != e.privateText)
            e.playerNotes[i / 2] +=
                roleName(state_.units[i].role) + L"：" + state_.units[i].report + L"\n";
    ++state_.revision;
    events_.push_back(e);
    snapshots_.push_back(state_);
    return true; // 每步记录版本号，联机拒绝重复或过期提交。
} // 单步执行结束。
