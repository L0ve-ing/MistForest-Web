namespace { // 人机分析只接收受限 View，不接触裁判和全知快照。
#include "knowledge.inc"
#include "threat.inc"

// Formation arithmetic is exact only when every living soldier's position is
// certain in this observer's belief. Never use the referee's hidden formation.
bool botKnownGuardCells(const View &v, const BotKnowledge &knowledge, int id,
                        std::array<int, 3> &cells) {
    for (int k = 0; k < 3; ++k) {
        if (v.units[id].soldierHealth[k] <= 0)
            continue;
        bool found = false;
        for (int c = 1; c <= cellCount(v.mode); ++c)
            if (knowledge.pieces[id][k][c - 1] >= 1 - 1e-9) {
                cells[k] = c;
                found = true;
                break;
            }
        if (!found)
            return false;
    }
    return true;
}

double botCellValue(const View &v, const BotKnowledge &knowledge, int id, int cell, double power,
                    bool ranged, bool execute = false) {
    if (cell < 1 || cell > cellCount(v.mode))
        return 0;
    const damage::Attack attack{power, ranged, execute, false};
    const auto &actor = v.units[id];
    const auto &building = v.buildings[cell - 1];
    bool sheltered = damage::buildingProtectsUnits(building, execute);
    double score = 0, intelDelta = 0;
    if (building.health > 0 && !allied(v.teams, building.owner, v.player) &&
        actor.role != Role::Wolf) {
        const auto result = damage::againstBuilding(building, actor.role, attack);
        double amount = building.health - result.healthAfter;
        if (amount > 1e-9)
            score += amount * 1.2 + (amount >= building.health - 1e-9 ? 1.2 : 0);
    }
    for (const auto &rose : v.roses)
        if (rose.cell == cell && rose.health > 0 && !allied(v.teams, rose.unit / 2, v.player)) {
            const auto result = damage::againstRose(rose, attack);
            if (result.hit.delta > 1e-9)
                score += (rose.health - result.healthAfter) * 1.5 +
                         (result.hit.delta >= rose.health - 1e-9 ? 1.5 : 0);
        }
    if (!sheltered)
        for (int enemy = 0; enemy < v.playerCount * 2; ++enemy) {
            const auto &u = v.units[enemy];
            if (u.health <= 0 || allied(v.teams, enemy / 2, v.player))
                continue;
            int pieces = u.role == Role::WallGuard ? 3 : 1;
            int alive = pieces == 3
                            ? int(std::count_if(u.soldierHealth.begin(), u.soldierHealth.end(),
                                                [](double hp) { return hp > 0; }))
                            : 1;
            std::array<int, 3> cells{};
            bool exactSquad = pieces == 3 && botKnownGuardCells(v, knowledge, enemy, cells);
            Unit squad;
            if (exactSquad) {
                squad = u;
                squad.soldiers = cells;
            }
            for (int k = 0; k < pieces; ++k) {
                const auto &target = exactSquad ? squad : u;
                double hp = pieces == 3 ? target.soldierHealth[k] : target.health;
                double probability = knowledge.pieces[enemy][k][cell - 1];
                if (hp <= 0 || probability <= 0)
                    continue;
                const auto result = damage::againstUnit(target, k, attack);
                if (result.hit.delta <= 1e-9)
                    continue;
                intelDelta = std::max(intelDelta, result.hit.delta);
                double body = hp - result.healthAfter;
                int kills = result.bodyDamage >= hp - 1e-9;
                if (exactSquad) {
                    auto sharing = !execute
                        ? damage::shareGuards(size(v.mode), squad.soldiers, squad.soldierHealth,
                                              k, result.bodyDamage)
                        : damage::GuardSharing{};
                    if (sharing.shared) {
                        body = 0;
                        kills = 0;
                        for (int n = 0; n < 3; ++n) {
                            body += squad.soldierHealth[n] - sharing.healthAfter[n];
                            kills += squad.soldierHealth[n] > 0 && sharing.healthAfter[n] <= 1e-9;
                        }
                        squad.soldierHealth = sharing.healthAfter;
                    } else
                        squad.soldierHealth[k] = result.healthAfter;
                    squad.armor = result.armorAfter;
                    squad.shield = result.shieldAfter;
                    squad.soldierDirectDamage[k] = result.hit.ledgerAfter;
                }
                if (execute) {
                    score += probability * (15 + 5 * alive) / alive;
                    continue;
                }
                score += probability * (2.5 * (result.armorDamage + result.shieldDamage) +
                                        body * 7 + kills * 8);
                score += probability * (1 - probability) * 0.9;
            }
        }
    if (v.intel[cell - 1] && !sheltered)
        score += 16 * std::min(2.0, intelDelta);
    return score;
}

// Projection records only public objects and targets whose sanitized belief is
// certain. Hidden hits and kills are never assumed; actual feedback updates the
// next decision. Public cell attack marks cannot suppress a different target.
void botProjectStrike(View &next, const BotKnowledge &knowledge, int actor, int target,
                      double power) {
    auto &building = next.buildings[target - 1];
    bool sheltered = damage::buildingProtectsUnits(building, false);
    if (sheltered && !allied(next.teams, building.owner, actor / 2) &&
        next.units[actor].role != Role::Wolf)
        building.directDamage =
            damage::againstBuilding(building, next.units[actor].role, {power}).hit.ledgerAfter;
    for (auto &rose : next.roses)
        if (rose.cell == target && rose.health > 0 && !allied(next.teams, rose.unit / 2, actor / 2))
            rose.directDamage = damage::againstRose(rose, {power}).hit.ledgerAfter;
    if (sheltered)
        return;
    for (int enemy = 0; enemy < next.playerCount * 2; ++enemy) {
        auto &u = next.units[enemy];
        if (u.health <= 0 || allied(next.teams, enemy / 2, actor / 2))
            continue;
        for (int k = 0; k < (u.role == Role::WallGuard ? 3 : 1); ++k)
            if (knowledge.pieces[enemy][k][target - 1] >= 1 - 1e-9) {
                double &ledger =
                    u.role == Role::WallGuard ? u.soldierDirectDamage[k] : u.directDamage;
                ledger = damage::againstUnit(u, k, {power}).hit.ledgerAfter;
            }
    }
}

double botDanger(const View &v, const BotKnowledge &knowledge, int id, int cell) {
    if (cell < 1 || cell > cellCount(v.mode))
        return 0;
    double danger = v.units[id].role != Role::Flamer && v.fires[cell - 1].expires ? 18 : 0;
    for (int enemy = 0; enemy < v.playerCount * 2; ++enemy)
        if (!allied(v.teams, enemy / 2, v.player) && v.units[enemy].health > 0) {
            double value = knowledge.threat[enemy][cell - 1] *
                           (v.units[enemy].role == Role::Sniper ? 2.5 : 1.2);
            if (v.buildings[cell - 1].health > 0 &&
                allied(v.teams, v.buildings[cell - 1].owner, v.player) &&
                v.units[enemy].role != Role::Sniper)
                value *= 0.55;
            danger += value;
        }
    return danger;
}

double botApproach(const View &v, const BotKnowledge &knowledge, int cell,
                   Role role) { // 靠近高权重候选区域，而非追逐真实坐标。
    double sum = 0, count = 0;
    for (int id = 0; id < v.playerCount * 2; ++id)
        if (!allied(v.teams, id / 2, v.player) && v.units[id].health > 0) { // 仅包含敌人。
            for (int c = 1; c <= cellCount(v.mode); ++c) {
                sum += knowledge.probability[id][c - 1] * (role == Role::Shadow
                                                               ? shadowDistance(v.mode, cell, c)
                                                               : distance(v.mode, cell, c));
            }
            ++count; // 曼哈顿距离只是接近评分，路径由 validate 检查。
        } // 多人按存活角色平均。
    return count ? sum / count : 0; // 没有敌人时不产生距离收益。
} // 接近评分结束。

double botPotential(const View &v, const BotKnowledge &knowledge, int id, int from,
                    const Unit &actor) { // 轻量预估该位置可能提供的攻击机会。
    double best = 0;
    auto single = [&](int cell, double power, bool ranged, bool execute = false) {
        return botCellValue(v, knowledge, id, cell, power, ranged, execute);
    }; // 按受击对象估计新增伤害。
    for (int target = 1; target <= cellCount(v.mode);
         ++target) { // 最多八十一格，避免搜索完整隐藏世界。
        int d = actor.role == Role::Shadow ? shadowDistance(v.mode, from, target)
                                           : distance(v.mode, from, target);
        double score = 0; // 不读取任何敌人落点。
        if (actor.role == Role::Flamer || actor.role == Role::Armiger ||
            actor.role == Role::Sword) { // 多格攻击沿真实地形和建筑生成。
            int length = actor.role == Role::Flamer    ? std::min(3, actor.ammo)
                         : actor.role == Role::Armiger ? 1 + int(std::ceil(actor.armor - 1e-9))
                                                       : 2; // 小数甲保持向上取整。
            if (actor.role == Role::Sword && actor.swordQi < 1) {
                continue;
            }
            for (int c : rayCells(v, from, target, length))
                score += single(c, 2, actor.role == Role::Flamer); // 剑气不足时没有普攻机会。
        } else if (actor.role == Role::Heavy) { // 后续冲锋的真正顺序在专用路径评分处理。
            if (d <= 2 && worldShot(v, from, target))
                score = single(target, 2, false) * 0.6; // 只作移动方向参考。
        } else {                                        // 普通攻击及盛放攻击范围。
            int range = actor.role == Role::Prince || actor.role == Role::Knight ? 1
                        : actor.role == Role::WallGuard
                            ? (guardWall(v.buildings[from - 1]) ? 2 : 1)
                            : attackRange(v.mode, actor); // 守卫军保留墙上射程。
            if (actor.role == Role::Ink)
                d = std::max(std::abs((from - 1) / size(v.mode) - (target - 1) / size(v.mode)),
                             std::abs((from - 1) % size(v.mode) -
                                      (target - 1) % size(v.mode))); // 白墨范围随落点改变。
            bool shot = actor.role == Role::Shadow && distance(v.mode, from, target) > 1
                            ? shadowShot(v.mode, from, target, v.map)
                            : worldShot(v, from, target); // 环绕射线保留原规则。
            if (d <= range && shot)
                score =
                    single(target, actor.role == Role::Ink ? 1 : 2,
                           actor.role == Role::Sniper || actor.role == Role::Ink ||
                               (actor.role == Role::HiddenWolf && !actor.form),
                           heroMode(v.mode) && actor.role == Role::Sniper); // 绝影仅职业模式生效。
        } // 几何机会不是提前命中反馈。
        best = std::max(best, score); // 选该位置最有价值的潜在攻击。
    } // 机会枚举结束。
    return best; // 不改变动作预算。
} // 位置机会结束。

double botChargeFuture(View v, const BotKnowledge &knowledge, int id,
                       int depth) { // 最多三步的己方冲锋路径展望。
    if (depth <= 0 || v.budget.used >= 4) {
        return 0;
    }
    double best = 0;
    int from = v.units[id].position; // 不模拟任何隐藏目标命中。
    for (int target = 1; target <= cellCount(v.mode);
         ++target) { // 继续使用原裁判校验转向、马路和建筑。
        Action a{id, Kind::Charge, target};
        if (!validate(v, a))
            continue;
        View next = v;
        next.units[id].position = target;
        next.budget.dr = (target - 1) / size(v.mode) - (from - 1) / size(v.mode);
        next.budget.dc = (target - 1) % size(v.mode) - (from - 1) % size(v.mode);
        next.budget.phase = 2;
        ++next.budget.used;
        next.budget.active = id; // 只预测自己的路径预算。
        double score = next.budget.used >= 2 ? botCellValue(v, knowledge, id, target, 2, false)
                                             : 0; // 第一步没有伤害。
        bool stops = v.buildings[target - 1].health && !canEnter(v, id, target);
        if (next.budget.used >= 2)
            botProjectStrike(next, knowledge, id, target, 2); // 重复格不得重复计分。
        if (!stops) {
            score += 0.8 * botChargeFuture(next, knowledge, id, depth - 1);
        }
        best = std::max(best, score); // 撞建筑后不预估继续穿行。
    } // 允许提前停止，不要求盲目走完四步。
    return best; // 最长路径规模受地图和四步上限限制。
} // 冲锋展望结束。

#include "planning.inc"
} // 人机内部助手结束。

Action chooseAction(const View &v, std::mt19937 &rng,
                    BotStyle style) { // 每一步重新决策，信息来源严格限制为 View。
    if (v.reaction.unit >= 0) {
        int recipient = -1;
        for (int k = 0; k < 3; ++k)
            if ((v.reaction.mask & (1 << k)) &&
                (recipient < 0 || v.reaction.health[k] > v.reaction.health[recipient]))
                recipient = k;
        return {v.reaction.unit, Kind::Share, 0, recipient, v.reaction.piece};
    } // 保留旧分担接口兼容。
    auto knowledge = botKnowledge(v);
    botPrepareThreat(v, knowledge);
    Action best{v.player * 2, Kind::End};
    double bestScore = -1e9;
    double caution = style == BotStyle::Cautious     ? 1.5
                     : style == BotStyle::Aggressive ? 0.55
                                                     : 1; // 三种风格都保留有效攻击能力。
    for (int id = v.player * 2; id < v.player * 2 + v.teamSize;
         ++id) { // 回合选角同时比较剩余两名角色的实际机会。
        const auto &u = v.units[id];
        if (u.health <= 0)
            continue;
        auto actions = choices(v, id);
        if (actions.empty())
            continue; // 已锁定角色由 validate 保证。
        std::array<double, 81> potential{}, danger{}, approach{};
        potential.fill(-1);
        danger.fill(-1);
        approach.fill(-1); // 缓存落点评分，控制单步延迟。
        auto attackAt = [&](int cell) {
            auto &value = potential[cell - 1];
            if (value < 0)
                value = botPotential(v, knowledge, id, cell, u);
            return value;
        }; // 同角色同落点只算一次。
        auto riskAt = [&](int cell) {
            auto &value = danger[cell - 1];
            if (value < 0)
                value = botDanger(v, knowledge, id, cell);
            return value;
        }; // 燃烧和反击风险缓存。
        auto distanceAt = [&](int cell) {
            auto &value = approach[cell - 1];
            if (value < 0)
                value = botApproach(v, knowledge, cell, u.role);
            return value;
        }; // 接近评分缓存。
        for (const auto &a : actions) { // 只考虑已由 validate 验证的动作及参数。
            if (a.kind == Kind::Surrender) {
                continue;
            }
            int from = soldierCell(u, a.piece);
            double score = -0.2, offenseValue = 0;
            bool offensive = false;
            auto hit = [&](int c, double power = 2, bool ranged = false, bool execute = false) {
                offensive = true;
                double value = botCellValue(v, knowledge, id, c, power, ranged, execute);
                offenseValue += value;
                return value;
            }; // 不主动投降。
            bool moving =
                a.target > 0 &&
                (a.kind == Kind::Move || a.kind == Kind::Lead || a.kind == Kind::Dash ||
                 a.kind == Kind::Leap || a.kind == Kind::Retreat || a.kind == Kind::FlameAdvance ||
                 a.kind == Kind::SwordAdvance || a.kind == Kind::FireStep ||
                 a.kind == Kind::CleaveRush || a.kind == Kind::AttackRush || a.kind == Kind::Hunt ||
                 a.kind == Kind::Bloom); // 位移和攻击可以同时存在。
            int landing =
                a.kind == Kind::Bloom ? a.landing : a.target; // 盛放真正落点与攻击格独立。
            if (a.kind == Kind::Dash)
                landing = dashPath(v, id, a.target).back();
            if (moving) { // 位置改变同时考虑安全、接近及下一步攻击机会。
                double continuation =
                    u.role == Role::Wolf || u.role == Role::Ink || u.role == Role::Armiger
                        ? 0.85
                        : 0.3; // 普通移动与普攻互斥职业的机会留给下一回合。
                if (u.role == Role::HiddenWolf && u.form && !v.budget.attacks &&
                    v.budget.moves == 0)
                    continuation = 0.85; // 狼形第一步后还能攻击。
                if (a.kind == Kind::Dash || a.kind == Kind::FlameAdvance ||
                    a.kind == Kind::FireStep)
                    continuation = 0.85; // 独立位移可创造本回合攻击机会。
                double followup =
                    botContinuation(botProject(v, a, knowledge), knowledge, id, a.piece);
                score +=
                    0.85 * followup - continuation * botLegalAttack(v, knowledge, id, a.piece) +
                    0.18 * attackAt(landing) +
                    0.35 *
                        (distanceAt(from) - distanceAt(landing)); // 本回合后续攻击先检查真实预算。
                score += caution * (riskAt(from) - riskAt(landing));
                if (landing == from)
                    score -= 2; // 无效原地移动不优先消耗资源。
                if (u.role == Role::WallGuard && guardWall(v.buildings[landing - 1]) &&
                    v.buildings[landing - 1].owner == v.player)
                    score += 1; // 登己方墙后利用抛射。
            } // 不改变玩家 UI 的可选落点。
            switch (a.kind) { // 职业能力使用统一伤害收益，再加入资源和后续机会。
            case Kind::Attack:
                score += hit(a.target, u.role == Role::Ink ? 1 : 2,
                             u.role == Role::Sniper || u.role == Role::Ink ||
                                 (u.role == Role::HiddenWolf && !u.form) ||
                                 (u.role == Role::WallGuard && guardWall(v.buildings[from - 1])),
                             heroMode(v.mode) && u.role == Role::Sniper);
                break; // 零差额攻击无额外固定加分。
            case Kind::AttackRush:
            case Kind::Cleave:
            case Kind::CleaveRush:
                score += hit(a.target);
                break; // 近战附带位移独立计算。
            case Kind::Hunt:
                score += hit(a.target) + 1.2;
                break; // 游猎落点后仍有后续技能。
            case Kind::Bloom:
                score += hit(a.target) - 0.1;
                break; // 花瓣是有限资源，优先攻击价值高的格。
            case Kind::Thrust:
            case Kind::Flame: { // 喷火枚数与刺击长度真实参与评分。
                auto cells =
                    rayCells(v, from, a.target,
                             a.kind == Kind::Flame ? a.amount : 1 + int(std::ceil(u.armor - 1e-9)));
                for (int c : cells)
                    score += hit(c, 2, a.kind == Kind::Flame); // 多格伤害独立汇总。
                if (a.kind == Kind::Flame) {
                    score -= 0.18 * a.amount + 0.4 * std::max(0, a.amount - int(cells.size()));
                }
                break; // 避免浪费被建筑截断的离火。
            } // 多格攻击结束。
            case Kind::SwordStab:
                for (int c : swordLine(v, from, a.target, a.amount != 0))
                    score += hit(c);
                score -= u.swordQi <= 2 ? 0.6 : 0.1;
                break; // 包含自身的剑刺也参与选择。
            case Kind::ThrowSword:
                score += hit(a.target) + 0.35 - 0.25 * a.amount;
                break; // 能普攻解决时保留剑气和整回合。
            case Kind::FireStep:
                if (a.amount)
                    score += hit(a.target, 2, true) - 1.6;
                else
                    score -= 0.15;
                break;           // 无收益点燃会损失本回合回复。
            case Kind::Charge: { // 第一步根据后续路径选方向，后续每步重新读取反馈。
                View next = v;
                next.units[id].position = a.target;
                next.budget.active = id;
                next.budget.phase = 2;
                ++next.budget.used;
                next.budget.dr = (a.target - 1) / size(v.mode) - (from - 1) / size(v.mode);
                next.budget.dc =
                    (a.target - 1) % size(v.mode) - (from - 1) % size(v.mode); // 只展望自己的预算。
                if (next.budget.used >= 2) {
                    score += hit(a.target);
                    botProjectStrike(next, knowledge, id, a.target, 2);
                } // 重复伤害格不加分。
                if (!v.buildings[a.target - 1].health || canEnter(v, id, a.target)) {
                    score += 0.8 * botChargeFuture(next, knowledge, id, 4 - next.budget.used);
                }
                score += 0.3 * (distanceAt(from) - distanceAt(a.target)) +
                         caution * (riskAt(from) - riskAt(a.target));
                break; // 不穿建筑做虚假预测。
            } // 冲锋评分结束。
            case Kind::Blood: { // 没有可探测敌人时不白白公开自己的位置。
                bool detectable = false;
                for (int j = 0; j < v.playerCount * 2; ++j)
                    if (!allied(v.teams, j / 2, v.player) && v.units[j].health > 0 &&
                        !immune(v.units[j]))
                        detectable = true;
                score += detectable ? 3.4 : -3;
                break; // 下一步再处理实际探测反馈。
            } // 嗜血只读取公开免疫形态。
            case Kind::ShadowMode:
                score += 3 + (a.amount == 2 ? 0.3 : 0);
                break; // 先明确模式，才使用相应动作。
            case Kind::Follow:
                score += a.subject / 2 == v.player ? -4 : 1.4;
                break; // 同玩家搭档本回合无法再行动，不盲目指定它。
            case Kind::Lead:
                score += a.subject / 2 == v.player ? -1 : 0.25;
                break;            // 未知目标阻挡只会在执行后得到反馈。
            case Kind::Silence: { // 控制范围概率独立于是否取得成功反馈。
                double covered = 0;
                for (int j = 0; j < v.playerCount * 2; ++j)
                    if (!allied(v.teams, j / 2, v.player) && v.units[j].health > 0)
                        for (int c = 1; c <= cellCount(v.mode); ++c)
                            if (distance(v.mode, v.budget.anchor, c) <= 1)
                                covered += knowledge.probability[j][c - 1];
                score += 0.8 + 4 * covered;
                break; // 保留逐步决定后续技能。
            } // 沉默不把未反馈解释成完全免疫。
            case Kind::Transform: { // 按攻防机会选形态，而非每回合固定变狼。
                Unit changed = u;
                changed.form = a.target;
                score += botPotential(v, knowledge, id, from, changed) - attackAt(from);
                if (!a.target)
                    score += 0.8 * caution;
                else
                    score += 0.25;
                break; // 人形临甲有防御价值。
            } // 基础能力仍由规则校验。
            case Kind::ArmorUp: { // 远离目标时避免反复堆甲丧失机动。
                Unit changed = u;
                changed.armor = std::min(2.0, std::ceil(u.armor + 1e-9));
                score += 0.6 * (botPotential(v, knowledge, id, from, changed) - attackAt(from)) +
                         1.2 * caution * (changed.armor - u.armor) -
                         0.35 * std::max(0.0, distanceAt(from) - 2);
                break; // 分数甲先补齐。
            } // 护甲不凭空增加。
            case Kind::ArmorDown: { // 卸甲只有解锁有价值的移动或攻击时才有收益。
                Unit changed = u;
                changed.armor -= armorUnload(u.armor, a.amount);
                View next = v;
                next.units[id] = changed;
                double opportunity =
                    botPotential(next, knowledge, id, from, changed); // 重算缩短后的刺击范围。
                if (int(std::ceil(changed.armor - 1e-9)) < int(std::ceil(u.armor - 1e-9)))
                    for (int c = 1; c <= cellCount(v.mode); ++c)
                        if (worldMove(next, id, from, c))
                            opportunity = std::max(
                                opportunity, botPotential(next, knowledge, id, c, changed) +
                                                 0.3 * (distanceAt(from) -
                                                        distanceAt(c))); // 一步即可利用的卸甲机会。
                score += 0.8 * (opportunity - attackAt(from)) - caution * (u.armor - changed.armor);
                break; // 不无目的耗尽防御。
            } // 小数卸甲使用裁判同一助手。
            case Kind::PlantRose: { // 种植替代移动，但可接盛放；评估新玫瑰周围落点。
                double bloom = botContinuation(botProject(v, a, knowledge), knowledge, id);
                score += hit(a.target, 1) + 0.8 * bloom + 1.1;
                score -= u.role != Role::Flamer && v.fires[from - 1].expires ? 18 : 0;
                break; // 种植后的盛放使用真实玫瑰及攻击额度。
            } // 不提前假设隐藏目标命中。
            case Kind::RecallSword: { // 召回必须留足剑刺所需的一点剑气。
                int cost = distance(v.mode, from, a.target);
                score += u.swordQi - cost >= 1 && v.budget.extraAttacks == 0
                             ? 0.6 + 0.6 * attackAt(from) - 0.15 * cost
                             : -1;
                break; // 多次召回不会无条件压过攻击。
            } // 额外普攻仍由规则实际消耗。
            case Kind::BuildFort:
            case Kind::BuildWall: { // 仅在攻击机会小且建筑能够使用时考虑防御。
                int owned = 0;
                for (const auto &b : v.buildings)
                    if (b.health && b.owner == v.player)
                        ++owned;
                score += u.role == Role::WallGuard ? 1.1 : 0.25;
                score -= 0.3 * owned + 0.2 * attackAt(from); // 避免全场无目的铺墙。
                for (int j = v.player * 2; j < v.player * 2 + v.teamSize; ++j)
                    if (v.units[j].health > 0)
                        for (int k = 0; k < (v.units[j].role == Role::WallGuard ? 3 : 1); ++k)
                            if (soldierCell(v.units[j], k) == a.target ||
                                (a.kind == Kind::BuildWall && u.role != Role::WallGuard &&
                                 soldierCell(v.units[j], k) == a.extra))
                                score -= 20; // 己方已知占格避免必然失败。
                if (v.fires[from - 1].expires && u.role != Role::Flamer) {
                    score -= 18;
                }
                break; // 不能用建造拖延自身燃烧。
            } // 隐藏占格仍只在实际执行时裁定。
            case Kind::Demolish:
                score += allied(v.teams, v.buildings[a.target - 1].owner, v.player) ? -6 : 1.5;
                break; // 避免拆己方防御，座狼已无此选项。
            case Kind::Row:
            case Kind::Col: { // 侦察版选择接近一半候选的分区。
                double p = 0;
                for (int j = 0; j < v.playerCount * 2; ++j)
                    if (!allied(v.teams, j / 2, v.player) && v.units[j].health > 0)
                        for (int c = 1; c <= cellCount(v.mode); ++c)
                            if ((a.kind == Kind::Row ? (c - 1) / size(v.mode)
                                                     : (c - 1) % size(v.mode)) == a.target)
                                p += knowledge.probability[j][c - 1];
                score += 1.6 * std::min(p, 1 - p);
                break; // 明确结果比重复已知分区有价值。
            } // 职业版没有侦察入口。
            case Kind::End:
                score -= riskAt(from);
                break; // 确定燃烧时不主动等待。
            default:
                break; // 普通位移和基础日字已由位置评分处理。
            } // 技能评分结束。
            if (offensive) {
                score += offenseValue > 1e-9 ? (style == BotStyle::Aggressive ? 0.35
                                                : style == BotStyle::Cautious ? -0.1
                                                                              : 0.1)
                                             : -0.5;
            } // 风格不压掉确定击杀，也不鼓励零新增伤害。
            score += std::uniform_real_distribution<double>(0, 0.08)(rng);
            if (score > bestScore) {
                bestScore = score;
                best = a;
            } // 微小随机扰动只打破接近的平分。
        } // 不提前提交整套动作，保留每步反馈。
    } // 最佳动作一定来自合法动作集合。
    return best; // 行动耗尽后结束，不无限执行无效技能。
} // v1.0.7 人机决策结束。
