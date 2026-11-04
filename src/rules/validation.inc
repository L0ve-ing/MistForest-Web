bool validateLegacy(const View &v, const Action &a,
                    std::wstring *why) { // 所有动作逐步提交，不能依赖敌方真实位置决定合法性。
    auto fail = [&](const wchar_t *s) {
        if (why)
            *why = s;
        return false;
    };
    auto ok = [&]() {
        if (why)
            why->clear();
        return true;
    }; // 集中清理错误文字。
    if (v.winner != -1 || v.turn != v.player || a.unit < 0 || a.unit >= 8 ||
        a.unit / 2 != v.player || v.units[a.unit].health <= 0)
        return fail(L"请选择自己的存活角色。"); // 玩家和存活状态限制。
    const auto &u = v.units[a.unit];
    const auto &b = v.budget;
    if (b.active >= 0 && b.active != a.unit)
        return fail(L"本回合已经行动，不能切换角色。"); // 一回合只允许一个角色主动行动。
    if (a.kind == Kind::End)
        return ok(); // 可提前结束，剩余次数放弃。
    if (int(a.kind) < 0 || int(a.kind) > int(Kind::End))
        return fail(L"未知动作。"); // 拒绝非法网络枚举。
    bool hero = heroMode(v.mode),
         skill = a.kind == Kind::Dash || a.kind == Kind::Follow || a.kind == Kind::Lead ||
                 a.kind == Kind::Transform ||
                 a.kind == Kind::Silence || a.kind == Kind::Cleave || a.kind == Kind::CleaveRush ||
                 a.kind == Kind::Retreat; // 普通攻击、基础日字移动不属于可沉默技能。
    if (skill && (!hero || u.silenced))
        return fail(L"当前不能使用该技能，沉默不妨碍普通攻击和基础移动。"); // 沉默逐角色独立生效。
    if (!hero) {
        if (b.used)
            return fail(L"本回合已行动，请结束回合。");
        if (a.kind == Kind::Move)
            return canMove(v.mode, u.position, a.target, v.map) ? ok()
                                                                : fail(L"移动只能相邻或原地。");
        if (a.kind == Kind::Attack)
            return distance(v.mode, u.position, a.target) <= 1 &&
                           clearShot(v.mode, u.position, a.target, v.map)
                       ? ok()
                       : fail(L"攻击目标不可达。");
        if ((a.kind == Kind::Row || a.kind == Kind::Col) && a.target >= 0 &&
            a.target < size(v.mode))
            return ok();
        return fail(L"侦察模式不使用职业技能。");
    } // 旧三格侦察独立保留。
    if (a.kind == Kind::Transform)
        return u.role == Role::HiddenWolf && !b.used && !b.transformed && a.target == 1 - u.form
                   ? ok()
                   : fail(L"每个己方回合只能在行动前切换一次。"); // 变身但锁定本回合角色。
    if (a.kind == Kind::Follow) {
        if (u.role != Role::Shadow || b.shadowMode != 2 || b.followed || b.led || b.moves > 2 ||
            a.subject < 0 || a.subject >= 8 || a.subject == a.unit ||
            v.units[a.subject].health <= 0 || immune(v.units[a.subject]))
            return fail(L"附骨需指定可跟随目标，指定后可移动2格。");
        return ok();
    } // 人形免疫不暴露任何隐藏坐标。
    if (u.role == Role::Knight) { // 骑士只有基础日字或强化游猎及后续选项。
        if (a.kind == Kind::Leap)
            return !b.used &&
                           (a.target == u.position || canLeap(v.mode, u.position, a.target, v.map))
                       ? ok()
                       : fail(
                             L"日字移动受马脚和墙限制；点击自身格可等待。"); // 沉默时仍允许基础日字或等待。
        if (a.kind == Kind::Hunt)
            return !b.used &&
                           (a.target == u.position || canLeap(v.mode, u.position, a.target, v.map))
                       ? ok()
                       : fail(L"倾轧必须作为第一项行动，跳到合法日字格。"); // 倾轧本体没有总冷却。
        int option = a.kind == Kind::Silence                                  ? 1
                     : (a.kind == Kind::Cleave || a.kind == Kind::CleaveRush) ? 2
                     : a.kind == Kind::Retreat ? 3
                                               : 0; // 三项固定按序结算。
        if (b.phase != 3 || !option || option <= b.lastOption || b.options >= 2 ||
            (option <= 2 && u.optionCooldown[option - 1]))
            return fail(
                L"后续选项按①②③顺序，最多两项，仅前两项需冷却完成。");
        if (option == 1)
            return ok(); // 沉默锚点来自已经公开的倾轧落点。
        if (option == 3)
            return canLeap(v.mode, u.position, a.target, v.map)
                       ? ok()
                       : fail(L"秘密跳跃的马脚、路径或落点被阻挡。"); // 第三项不造成落点伤害。
        if (distance(v.mode, u.position, a.target) > 1 ||
            !clearShot(v.mode, u.position, a.target, v.map))
            return fail(L"挥劈目标须在距离1内且无遮挡。"); // 湖泊可以攻击。
        if (a.kind == Kind::CleaveRush && !canMove(v.mode, u.position, a.target, v.map)) {
            return fail(L"突进不能跨墙或落入湖泊。");
        }
        return ok(); // 突进不因敌人站位受到阻挡。
    } // 骑士专属限制结束。
    if (a.kind == Kind::Charge) { // 重骑逐格推进，伤害格仅在结束后排序公开。
        if (u.role != Role::Heavy || b.phase == 1 || b.used >= 4 ||
            !canMove(v.mode, u.position, a.target, v.map) ||
            distance(v.mode, u.position, a.target) != 1)
            return fail(L"冲锋最多4步，不能混入普通移动或穿墙。"); // 冲锋不能原地。
        int dr = (a.target - 1) / size(v.mode) - (u.position - 1) / size(v.mode),
            dc = (a.target - 1) % size(v.mode) - (u.position - 1) % size(v.mode); // 计算本步方向。
        if (b.used && ((b.used > 1 && (dr != b.dr || dc != b.dc)) ||
                       (b.used == 1 && dr == -b.dr && dc == -b.dc))) {
            return fail(L"仅第一步后可转向一次，不能反向。");
        }
        return ok(); // 第三步起保持方向。
    } // 冲锋判断结束。
    if (a.kind == Kind::Dash)
        return u.role == Role::Ink && !u.cooldown &&
                       !dashPath(v.mode, u.position, a.target, v.map).empty()
                   ? ok()
                   : fail(L"炫空斗舞须冷却完成，沿一个方向最多冲刺2格，遇障停下。"); // 冲刺自身逐步处理附骨。
    if (a.kind == Kind::Move || a.kind == Kind::Lead) {                // 原地不动归入移动次数。
        if (!(u.role == Role::Shadow ? shadowStep(v.mode, u.position, a.target, v.map)
                                     : canMove(v.mode, u.position, a.target, v.map)))
            return fail(L"请选择相邻可站立格或自身格。"); // 墙与湖泊限制普通移动。
        if (a.kind == Kind::Lead && (u.role != Role::Shadow || b.shadowMode != 1 || b.followed ||
                                     a.subject < 0 || a.subject >= 8 || a.subject == a.unit ||
                                     v.units[a.subject].health <= 0 || immune(v.units[a.subject])))
            return fail(L"请选择可被附骨带动的目标。"); // 主动带动只影响指定一人。
        bool allowed = false;
        if (u.role == Role::Sniper)
            allowed = !b.used;
        if (u.role == Role::Wolf)
            allowed = b.used < 3;
        if (u.role == Role::Ink)
            allowed = b.moves < 1;
        if (u.role == Role::Shadow)
            allowed = !b.attacks && b.moves < 2;
        if (u.role == Role::Shadow && b.shadowMode == 0 && !u.silenced)
            allowed = false;
        if (u.role == Role::Heavy)
            allowed = b.phase != 2 && b.moves < 2; // 各职业原有移动预算。
        if (u.role == Role::HiddenWolf)
            allowed = u.form ? (b.attacks ? b.instinct : b.moves < 2)
                             : !b.used; // 狼形攻击后只使用独立的本能一步。
        return allowed
                   ? ok()
                   : fail(
                         L"移动额度已用完，或不能与本回合动作组合。"); // 次数不足时不发生任何状态变化。
    } // 移动判断结束。
    if (a.kind == Kind::Attack) {
        if (u.role == Role::Shadow && b.shadowMode == 0 && !u.silenced)
            return fail(L"先选择本回合附骨模式。"); // 普通攻击不被沉默禁用，绝影被动持续有效。
        if (u.role == Role::Heavy ||
            (u.role == Role::Ink
                 ? std::max(
                       std::abs((u.position - 1) / size(v.mode) - (a.target - 1) / size(v.mode)),
                       std::abs((u.position - 1) % size(v.mode) - (a.target - 1) % size(v.mode)))
                 : (u.role == Role::Shadow ? shadowDistance(v.mode, u.position, a.target)
                                           : distance(v.mode, u.position, a.target))) >
                attackRange(v.mode, u) ||
            !(u.role == Role::Shadow && distance(v.mode, u.position, a.target) > 1
                  ? shadowShot(v.mode, u.position, a.target, v.map)
                  : clearShot(v.mode, u.position, a.target, v.map)))
            return fail(
                L"该角色不能普通攻击，或目标超出射程、被墙遮挡。"); // 几何检查不接触敌方坐标。
        bool allowed = u.role == Role::Wolf  ? b.used < 3
                       : u.role == Role::Ink ? b.pellets < inkPelletLimit(b)
                       : u.role == Role::HiddenWolf
                           ? (u.form ? !b.attacks && b.moves <= 1 : !b.used)
                           : !b.attacks && !b.moves;         // 座狼三次自由行动，白墨先后移动不限。
        return allowed ? ok() : fail(L"本回合无法再攻击。"); // 命中不影响合法目标枚举。
    } // 攻击判断结束。
    return fail(L"当前角色不支持此动作。"); // 不允许职业版侦察等旧动作。
} // 单步校验结束。
