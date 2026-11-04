// The panel reads displayed snapshots while input checks use the live referee.
constexpr int ActionTop = 314, ActionHeight = 234, ActionStep = 58;

struct Operation {
    int id;
    Kind kind;
    std::wstring title, detail;
    bool toggle = false;
};

std::vector<Operation> operations(const View &v) {
    const auto &u = v.units[s.choice.state().unit];
    std::vector<Operation> out;
    auto add = [&](int id, Kind kind, const wchar_t *title, const wchar_t *detail,
                   bool toggle = false) { out.push_back({id, kind, title, detail, toggle}); };
    if (u.role == Role::Knight && heroMode(v.mode)) {
        if (v.budget.phase == 3) {
            add(Silence, Kind::Silence, L"① 沉默与易伤", L"距离 1 · 易伤 +0.5 · 最多选两项");
            add(Cleave, s.choice.state().lunge ? Kind::CleaveRush : Kind::Cleave, L"② 挥劈",
                L"距离 1 · 伤害 1");
            add(Retreat, Kind::Retreat, L"③ 秘密跳跃", L"日字跳跃 · 不造成倾轧伤害");
        } else {
            add(Leap, Kind::Leap, L"日字移动", L"检查马腿与落点 · 与游猎二选一");
            add(Hunt, Kind::Hunt, L"普通攻击 · 倾轧", L"伤害 1 · 随后按序选择强化");
        }
        return out;
    }
    add(Move, Kind::Move, L"移动",
        u.role == Role::Wolf ? L"每回合 3 次行动 · 逐步选路" : L"按角色移动额度 · 选路径后执行");
    if (!heroMode(v.mode)) {
        add(Attack, Kind::Attack, L"攻击", L"选择目标格");
        add(Row, Kind::Row, L"侦察一行", L"获得这一行的探测结果");
        add(Col, Kind::Col, L"侦察一列", L"获得这一列的探测结果");
        return out;
    }
    switch (u.role) {
    case Role::Prince:
        add(PlantRose, Kind::PlantRose, L"玫瑰之吻", L"距离 2 · 伤害 0.5 · 替代移动");
        add(Bloom, Kind::Bloom, L"荆棘盛放", L"选玫瑰、落点、攻击格 · 伤害 1");
        break;
    case Role::Sword:
        add(SwordStab, Kind::SwordStab, L"剑气直刺", L"两格 · 伤害 1 · 消耗 1 剑气");
        add(ThrowSword, Kind::ThrowSword, L"飞剑", L"距离 0–3 · 原地免费 · 可立即召回");
        add(RecallSword, Kind::RecallSword, L"召回", L"按距离支付剑气 · 获得额外普攻或移动");
        break;
    case Role::Heavy:
        add(Charge, Kind::Charge, L"冲锋", L"最多 4 步 · 第 2–4 步伤害 1");
        break;
    case Role::Armiger:
        add(Thrust, Kind::Thrust, L"普通攻击 · 刺击", L"可移动后刺击 · 护甲决定射程");
        add(ArmorUp, Kind::ArmorUp, L"负甲", L"占整回合 · 护甲上限 2");
        add(ArmorDown, Kind::ArmorDown, L"卸甲", L"选择层数 · 回合开始时卸甲");
        break;
    case Role::Flamer:
        add(Flame, Kind::Flame, L"火舞 · 喷射", L"消耗 1–3 离火 · 每格伤害 1");
        add(FireStep, Kind::FireStep, L"火步", L"额外移动 1 格 · 消耗 1 离火");
        break;
    default:
        add(Attack,
            u.role == Role::Alpha && s.choice.state().rush ? Kind::AttackRush : Kind::Attack,
            u.role == Role::Wolf     ? L"撕咬"
            : u.role == Role::Ink    ? L"炫空斗舞 · 霰弹"
            : u.role == Role::Sniper ? L"绝影"
                                     : L"攻击",
            u.role == Role::Ink                     ? L"每发 0.5 · 最多 3 发 · 周围八格"
            : u.role == Role::Sniper                ? L"距离 3 · 命中必杀 · 与移动二选一"
            : u.role == Role::HiddenWolf && !u.form ? L"人形 · 距离 2 · 远程伤害 1"
            : u.role == Role::WallGuard && soldierCell(u, s.choice.state().piece) > 0 &&
                    guardWall(v.buildings[soldierCell(u, s.choice.state().piece) - 1])
                ? L"站墙抛射 · 距离 2 · 伤害 1"
                : L"距离 1 · 伤害 1");
        if (u.role == Role::Ink)
            add(Dash, Kind::Dash, L"冲刺", L"恰好 2 格 · 造成伤害刷新冷却");
        if (u.role == Role::HiddenWolf)
            add(Transform, Kind::Transform, u.form ? L"切换人形" : L"切换狼形",
                L"每回合行动前一次 · 无变身冷却");
        if (u.role == Role::Alpha)
            add(Blood, Kind::Blood, L"嗜血 · 探测", L"回合首个动作 · 冷却 2 己方回合");
        if (u.role == Role::Shadow) {
            if (v.budget.shadowMode == 0) {
                add(ShadowLeadMode, Kind::ShadowMode, L"主动带动模式", L"回合首个动作 · 与跟随互斥",
                    true);
                add(ShadowFollowMode, Kind::ShadowMode, L"指定跟随模式",
                    L"回合首个动作 · 与带动互斥", true);
            } else {
                add(Follow, Kind::Follow, L"指定跟随", L"目标移动时复制 · 选择对象");
                add(Lead, Kind::Lead, L"附骨 · 带动", L"移动时带动对象 · 选择对象");
            }
        }
        break;
    }
    return out;
}

bool operationEnabled(const View &v, const Operation &op) {
    if (op.id == Transform)
        return validate(
            v, {s.choice.state().unit, Kind::Transform, 1 - v.units[s.choice.state().unit].form});
    if (op.toggle)
        return validate(v, {s.choice.state().unit, Kind::ShadowMode, 0, -1, 0, 0,
                            op.id == ShadowLeadMode ? 1 : 2});
    return canKind(v, op.kind);
}

void meter(HDC dc, const std::wstring &label, double value, double maximum, RECT r, COLORREF tint) {
    text(dc, label, rect(r.left, r.top, 65, 22), 15, INK, true);
    text(dc, halfPoints(value) + L" / " + halfPoints(maximum),
         rect(r.left + 67, r.top, r.right - r.left - 67, 22), 16, tint, true,
         DT_RIGHT | DT_SINGLELINE);
    fill(dc, rect(r.left, r.top + 27, r.right - r.left, 5), LINE);
    int width = int((r.right - r.left) * std::clamp(value / std::max(1., maximum), 0., 1.));
    if (width)
        fill(dc, rect(r.left, r.top + 27, width, 5), tint);
}

void characterStatus(HDC dc, const View &v, bool play) {
    const auto &u = v.units[s.choice.state().unit];
    portrait(dc, s.choice.state().unit, rect(1068, 107, 108, 108), v);
    meter(dc, L"生命", u.health, initialHealth(u.role, v.mode), rect(1192, 111, 265, 35), CORAL);
    text(dc, L"护甲  " + halfPoints(u.armor * 2), rect(1192, 158, 126, 23), 16, TEAL, true);
    text(dc, L"临盾  " + halfPoints(u.shield), rect(1320, 158, 137, 23), 16, TEAL, true);
    fill(dc, rect(1192, 190, 265, 4), LINE);
    double defense = u.armor * 2 + u.shield;
    if (defense > 0)
        fill(dc, rect(1192, 190, int(265 * std::min(1., defense / 4.)), 4), TEAL);
    std::wstring resource =
        u.role == Role::Sword
            ? L"剑气 " + std::to_wstring(u.swordQi) + L" / " +
                  std::to_wstring(9 - int(std::count_if(v.swords.begin(), v.swords.end(),
                                                        [&](const Sword &x) {
                                                            return x.unit == s.choice.state().unit;
                                                        })))
        : u.role == Role::Flamer     ? L"离火 " + std::to_wstring(u.ammo) + L" / 3"
        : u.role == Role::Ink        ? L"霰弹 " + std::to_wstring(v.budget.pellets) + L" / 3"
        : u.role == Role::HiddenWolf ? (u.form ? L"狼形 · 近战减伤 50%" : L"人形 · 远程减伤 50%")
                                     : L"头像右键查看介绍 · H 查看技能";
    text(dc, resource, rect(1068, 213, 389, 24), 14, MUTED);
    auto actual = liveView();
    int x = 1068;
    for (int k = 0; k < v.teamSize; ++k) {
        int id = s.viewer * 2 + k;
        if (id == s.choice.state().unit)
            continue;
        bool enabled = play && v.units[id].health > 0 && actual.budget.active < 0;
        characterAvatar(dc, rect(x, 244, 46, 46), v.units[id].role, v.units[id].form, true, false);
        s.hits.push_back({rect(x, 244, 46, 46), Unit0 + k, enabled});
        text(dc,
             v.units[id].health <= 0     ? L"阵亡"
             : actual.budget.active >= 0 ? L"锁定"
                                         : L"切换",
             rect(x + 53, 255, 58, 22), 14, enabled ? TEAL : MUTED, true);
        x += 116;
    }
    button(dc, CycleRole, L"切换角色", rect(1305, 248, 152, 32), false,
           play && nextSelectableUnit(liveView()) >= 0, 16);
    text(dc,
         presentationBusy()          ? L"正在播放 · 完成后开放操作"
         : s.phase == Phase::Playing ? L"己方行动 · 第 " + std::to_wstring(v.roundNumber) + L" 轮"
                                     : L"等待其他玩家行动",
         rect(1068, 293, 389, 20), 13, TEAL, true);
}

void operationCard(HDC dc, const View &v, const Operation &op, int y, bool play) {
    bool available = operationEnabled(v, op), enabled = play && available,
         selected = s.choice.state().kind == op.kind && !op.toggle,
         hovered = enabled && visual.hover == op.id;
    RECT r = rect(1066, y, 393, 52);
    card(dc, r,
         selected  ? PALE
         : enabled ? (hovered ? RGB(248, 247, 232) : RGB(253, 251, 240))
                   : RGB(233, 237, 230),
         selected ? TEAL : LINE, 12, selected ? 2 : 1);
    if (selected)
        fill(dc, rect(r.left + 1, y + 9, 3, 34), TEAL);
    if (op.kind == Kind::Move || op.kind == Kind::Leap) {
        HPEN pen = CreatePen(PS_SOLID, 2, enabled ? TEAL : MUTED);
        auto old = SelectObject(dc, pen);
        MoveToEx(dc, 1083, y + 34, nullptr);
        LineTo(dc, 1100, y + 17);
        LineTo(dc, 1100, y + 29);
        MoveToEx(dc, 1100, y + 17, nullptr);
        LineTo(dc, 1088, y + 17);
        SelectObject(dc, old);
        DeleteObject(pen);
    } else
        drawArt(dc, rect(1075, y + 10, 32, 32),
                op.kind == Kind::Attack || op.kind == Kind::Cleave || op.kind == Kind::SwordStab
                    ? 17
                    : int(v.units[s.choice.state().unit].role),
                enabled ? 1.f : .45f);
    text(dc, op.title, rect(1118, y + 5, 282, 23), 18, enabled ? INK : MUTED, true, DT_SINGLELINE);
    std::wstring detail = op.detail;
    const auto &u = v.units[s.choice.state().unit];
    if (!play)
        detail = presentationBusy() ? L"等待当前行动与反馈显示完毕" : L"等待己方行动";
    else if (!available) {
        if (u.silenced && op.kind != Kind::Move && op.kind != Kind::Attack &&
            op.kind != Kind::AttackRush && op.kind != Kind::Leap && op.kind != Kind::Charge &&
            op.kind != Kind::Hunt && op.kind != Kind::Thrust && op.kind != Kind::SwordStab)
            detail = L"沉默期间不可使用";
        else if (op.kind == Kind::RecallSword &&
                 std::none_of(v.swords.begin(), v.swords.end(),
                              [&](const Sword &x) { return x.unit == s.choice.state().unit; }))
            detail = L"当前没有可召回的飞剑";
        else if (u.role == Role::Sword && u.swordQi <= 0)
            detail = L"剑气不足";
        else if (u.cooldown && (op.kind == Kind::Dash || op.kind == Kind::Blood))
            detail = L"冷却 " + std::to_wstring(u.cooldown) + L" 个己方回合";
        else if (u.role == Role::Flamer && u.ammo <= 0)
            detail = L"离火不足";
        else if (op.kind == Kind::ArmorUp && u.armorLock)
            detail = L"受击后暂时不能负甲";
        else if (op.toggle || op.kind == Kind::Transform || op.kind == Kind::Blood)
            detail = L"仅能作为回合首个动作";
        else if (op.kind == Kind::Move && v.budget.moves > 0)
            detail = L"本回合移动额度已用完";
        else if (v.budget.attacks > 0)
            detail = L"当前攻击额度或阶段已结束";
        else if (u.role == Role::Shadow && v.budget.shadowMode == 0)
            detail = L"请先选择带动或跟随模式";
        else
            detail = L"当前行动阶段不可使用";
    }
    text(dc, detail, rect(1118, y + 30, 327, 18), 12, enabled ? MUTED : RGB(130, 138, 127), false,
         DT_SINGLELINE);
    int key = shortcut(op.id);
    if (key) {
        card(dc, rect(1415, y + 6, 31, 23), enabled ? PALE : LINE, LINE, 6);
        text(dc, std::wstring(1, wchar_t(key)), rect(1415, y + 7, 31, 20), 14,
             enabled ? TEAL : MUTED, true, DT_CENTER | DT_SINGLELINE);
    }
    s.hits.push_back({r, op.id, enabled});
}

std::wstring selectionHint(const View &v) {
    if (presentationBusy())
        return L"等待行动、特效与反馈完整显示。";
    if (!s.notice.empty())
        return s.notice;
    if (s.choice.state().kind == Kind::SwordStab)
        return s.choice.state().swordIncludesSelf ? L"点相邻格选方向：攻击自身与相邻格。"
                                                  : L"点相邻格选方向：攻击前方连续两格。";
    if (s.choice.state().kind == Kind::Bloom)
        return !s.choice.state().extra          ? L"第一步：选择玫瑰。"
               : !s.choice.state().bloomLanding ? L"第二步：选择瞬移落点。"
                                                : L"第三步：选择攻击格，按空格。";
    if (s.choice.state().kind == Kind::BuildWall &&
        v.units[s.choice.state().unit].role != Role::WallGuard)
        return !s.choice.state().selected ? L"选择身旁两格城墙，不包含自身。"
               : !s.choice.state().extra  ? L"再选相邻的第二格城墙。"
                                          : L"已选两格；失败也会结束回合。";
    return s.choice.state().selected
               ? L"目标：" + coordinate(v.mode, s.choice.state().selected) + L" · 空格执行"
           : targetlessKind(s.choice.state().kind) ? L"参数选好后，按空格执行。"
                                                   : L"选择行动 → 点目标格 → 空格执行";
}

void battleOperations(HDC dc, const View &v) {
    bool play = battleInputReady();
    const auto &u = v.units[s.choice.state().unit];
    characterStatus(dc, v, play);
    if (!presentationBusy() && pendingFollowup(liveView())) {
        card(dc, rect(1066, 365, 393, 210), PALE);
        text(dc, L"攻击完成 · 选择后续落点", rect(1081, 384, 362, 45), 24, TEAL, true);
        text(dc,
             v.budget.swordAdvance ? L"点击刚才攻击过的合法格，进入攻击格；也可以留在原地。"
                                   : L"点击正在燃烧的合法格，进入火焰；也可以留在原地。",
             rect(1081, 447, 362, 96), 20, INK);
        text(dc, L"落点仅己方可见 · 等待反馈显示完毕", rect(1068, 614, 389, 45), 16, MUTED);
        button(dc, Commit, L"进入所选格 · 空格", rect(1066, 792, 393, 37), true,
               play && s.choice.state().selected &&
                   validate(v, selectedAction(v, s.choice.state().selected)),
               19);
        button(dc, StayAfterAttack, L"留在原地", rect(1066, 861, 393, 30), false, play, 18);
        return;
    }
    auto list = operations(v);
    s.actionScroll = std::clamp(s.actionScroll, 0, std::max(0, int(list.size()) - 4));
    for (int i = 0; i < std::min(4, int(list.size()) - s.actionScroll); ++i)
        operationCard(dc, v, list[s.actionScroll + i], ActionTop + i * ActionStep, play);
    if (u.role == Role::WallGuard)
        for (int k = 0; k < 3; ++k)
            button(dc, Piece0 + k,
                   L"士兵" + std::to_wstring(k + 1) + L" · " + halfPoints(u.soldierHealth[k]),
                   rect(1066 + k * 134, 563, 125, 28), s.choice.state().piece == k,
                   play && u.soldierHealth[k] > 0 && !(v.budget.squadDone & (1 << k)), 15);
    if (u.role == Role::Sword) {
        if (s.choice.state().kind == Kind::SwordStab)
            button(dc, IncludeSelf,
                   s.choice.state().swordIncludesSelf ? L"包含自身：开" : L"包含自身：关",
                   rect(1066, 563, 393, 28), s.choice.state().swordIncludesSelf, play, 16);
        if (s.choice.state().kind == Kind::ThrowSword) {
            int cost = s.choice.state().selected
                           ? distance(v.mode, u.position, s.choice.state().selected)
                           : 0;
            text(dc, L"按距离消耗剑气：" + std::to_wstring(cost) + L"（原地为0）",
                 rect(1066, 567, 393, 24), 16);
        }
    }
    if (u.role == Role::Flamer) {
        for (int k = 1; k <= 3; ++k)
            button(dc, Amount1 + k - 1, std::to_wstring(k) + L" 离火",
                   rect(1066 + (k - 1) * 134, 563, 125, 28), s.choice.state().amount == k,
                   play && parameterAvailable(v, k), 15);
        button(dc, IgniteToggle, s.choice.state().ignite ? L"火步点燃：开" : L"火步点燃：关",
               rect(1066, 596, 393, 25), s.choice.state().ignite,
               play && canKind(v, Kind::FireStep), 15);
    }
    if (u.role == Role::Armiger && s.choice.state().kind == Kind::ArmorDown) {
        double fraction = u.armor - std::floor(u.armor + 1e-9);
        for (int k = 0; k < 2; ++k) {
            int choice = k + (fraction > 1e-9 ? 0 : 1);
            button(dc, Amount1 + k, halfPoints((fraction + choice) * 2) + L" 层",
                   rect(1066 + k * 202, 563, 190, 28), s.choice.state().amount == choice,
                   play && parameterAvailable(v, choice), 15);
        }
    }
    if (u.role == Role::Knight && v.budget.phase == 3)
        button(dc, Lunge, s.choice.state().lunge ? L"挥劈后突进：开" : L"挥劈后突进：关",
               rect(1066, 563, 393, 28), s.choice.state().lunge,
               play && (canKind(v, Kind::Cleave) || canKind(v, Kind::CleaveRush)), 16);
    if (u.role == Role::Alpha)
        button(dc, RushToggle, s.choice.state().rush ? L"攻击突进：开" : L"攻击突进：关",
               rect(1066, 563, 393, 28), s.choice.state().rush,
               play && (canKind(v, Kind::Attack) || canKind(v, Kind::AttackRush)), 16);
    if (u.role == Role::Shadow &&
        (s.choice.state().kind == Kind::Follow || s.choice.state().kind == Kind::Lead)) {
        int index = 0;
        for (int id = 0; id < v.playerCount * 2; ++id)
            if (id != s.choice.state().unit && id % 2 < v.teamSize) {
                button(dc, TargetBase + id, playerName(id / 2) + L"·" + roleName(v.units[id].role),
                       rect(1066 + (index % 3) * 134, 559 + (index / 3) * 21, 125, 19),
                       s.choice.state().subject == id, play && v.units[id].health > 0, 12);
                ++index;
            }
        if (v.units[s.choice.state().subject].role == Role::WallGuard)
            button(dc, EnemyPiece, L"附骨士兵" + std::to_wstring(s.choice.state().enemyPiece + 1),
                   rect(1066, 625, 125, 28), false, play, 14);
    }
    bool targetSoldier =
        u.role == Role::Shadow &&
        (s.choice.state().kind == Kind::Follow || s.choice.state().kind == Kind::Lead) &&
        v.units[s.choice.state().subject].role == Role::WallGuard;
    if (u.role != Role::Flamer &&
        !(u.role == Role::Shadow &&
          (s.choice.state().kind == Kind::Follow || s.choice.state().kind == Kind::Lead)))
        text(dc, L"建造与拆除", rect(1068, 601, 389, 20), 13, TEAL, true);
    if (heroMode(v.mode) && !targetSoldier) {
        auto facility = [&](int id, Kind kind, const wchar_t *label, RECT r) {
            button(dc, id, label, r, s.choice.state().kind == kind, play && canKind(v, kind), 15);
        };
        if (u.role != Role::WallGuard)
            facility(Fort, Kind::BuildFort, L"堡垒", rect(1066, 625, 125, 28));
        facility(WallBuild, Kind::BuildWall, u.role == Role::WallGuard ? L"建造长城" : L"城墙",
                 rect(u.role == Role::WallGuard ? 1066 : 1200, 625,
                      u.role == Role::WallGuard ? 259 : 125, 28));
        facility(Demolish, Kind::Demolish, L"拆除", rect(1334, 625, 125, 28));
    }
    card(dc, rect(1066, 662, 393, 46), PALE);
    text(dc, statusText(v), rect(1075, 667, 375, 37), 13, TEAL);
    text(dc, selectionHint(v), rect(1072, 751, 381, 36), 14, MUTED);
    if (s.phase == Phase::Hold)
        button(dc, Pass, L"隐藏视角 · 交给下一位", rect(1066, 792, 393, 37), true,
               !presentationBusy(), 19);
    else
        button(dc, Commit,
               presentationBusy() ? L"播放结束后可行动"
               : s.choice.state().planned.empty()
                   ? L"执行所选行动 · 空格"
                   : L"执行已选 " + std::to_wstring(s.choice.state().planned.size()) + L" 步",
               rect(1066, 792, 393, 37), true,
               play && (!s.choice.state().planned.empty() ||
                        ((targetlessKind(s.choice.state().kind) || s.choice.state().selected) &&
                         validate(v, selectedAction(v, s.choice.state().selected)))),
               19);
    button(dc, BatchToggle, s.choice.state().batching ? L"批量：开" : L"批量：关",
           rect(1066, 834, 151, 22), s.choice.state().batching, play, 14);
    button(dc, BatchClear, L"清空路径", rect(1225, 834, 151, 22), false,
           play && !s.choice.state().planned.empty(), 14);
    button(dc, MoreActions, L"更多…", rect(1384, 834, 75, 22), false, play, 13);
    button(dc, EndTurn, L"结束回合", rect(1066, 861, 393, 30), false, play, 19);
}

void portraitTip(HDC dc) {
    if (s.page != Page::Battle || s.inspected >= 0 || s.pendingLeave || s.tutorial || !hasView())
        return;
    int id = visual.hover;
    int unit = id >= DetailsBase && id < DetailsBase + 8 ? id - DetailsBase
               : id == Unit0 || id == Unit1              ? s.viewer * 2 + id - Unit0
                                                         : -1;
    if (unit < 0)
        return;
    auto v = displayedView();
    std::wstring label = roleName(v.units[unit].role) + L" · 右键查看技能";
    if (id == Unit0 || id == Unit1)
        label += v.budget.active >= 0 ? L" · 本回合已锁定" : L" · 点击切换";
    card(dc, rect(1066, 310, 393, 31), WHITE, GOLD, 8);
    text(dc, label, rect(1074, 316, 376, 20), 14, TEAL, true, DT_SINGLELINE);
}
