std::vector<Action> choices(const View &v, int id) { // 只列举该玩家可知信息下的合法动作。
    std::vector<Action> out;
    if (id < 0 || id >= 8 || v.units[id].health <= 0 || id / 2 != v.player)
        return out;
    const auto &u = v.units[id];
    for (int piece = 0; piece < (u.role == Role::WallGuard ? 3 : 1); ++piece)
        for (int k = 0; k <= int(Kind::End); ++k) {
            Kind kind = Kind(k);
            for (int c = 0; c <= cellCount(v.mode); ++c) {
                if ((kind == Kind::End || kind == Kind::Silence || kind == Kind::Blood ||
                     kind == Kind::ArmorUp || kind == Kind::ArmorDown ||
                     kind == Kind::ShadowMode) &&
                    c > 0)
                    break;
                Action a{id, kind, c, -1, piece};
                if (kind == Kind::Follow || kind == Kind::Lead) {
                    for (int target = 0; target < 8; ++target) {
                        a.subject = target;
                        for (int part = 0; part < (v.units[target].role == Role::WallGuard ? 3 : 1);
                             ++part) {
                            a.extra = part;
                            if (validate(v, a))
                                out.push_back(a);
                        }
                    }
                    continue;
                } // 附骨可选择任意存活士兵，不局限第一名。
                if (kind == Kind::Flame) {
                    for (int amount = 1; amount <= std::min(3, u.ammo); ++amount) {
                        a.amount = amount;
                        if (validate(v, a))
                            out.push_back(a);
                    }
                    continue;
                } // 人机按资源枚举一至三枚离火。
                if (kind == Kind::FireStep) {
                    for (int ignite : {0, 1}) {
                        a.amount = ignite;
                        if (validate(v, a))
                            out.push_back(a);
                    }
                    continue;
                } // 同时提供纯位移和点燃位移。
                if (kind == Kind::BuildWall && u.role != Role::WallGuard) {
                    for (int second = 1; second <= cellCount(v.mode); ++second) {
                        a.extra = second;
                        if (validate(v, a))
                            out.push_back(a);
                    }
                    continue;
                }
                if (kind == Kind::ArmorDown) {
                    for (int choice : {0, 1, 2}) {
                        a.amount = choice;
                        if (validate(v, a))
                            out.push_back(a);
                    }
                    continue;
                }
                if (kind == Kind::ShadowMode) {
                    for (int m : {1, 2}) {
                        a.amount = m;
                        if (validate(v, a))
                            out.push_back(a);
                    }
                    continue;
                }
                if (kind == Kind::Bloom) {
                    for (const auto &rose : v.roses)
                        if (rose.unit == id && rose.health > 0) {
                            a.extra = rose.cell;
                            for (int landing = 1; landing <= cellCount(v.mode); ++landing)
                                if (distance(v.mode, rose.cell, landing) <= 1) {
                                    a.landing = landing;
                                    if (validate(v, a))
                                        out.push_back(a);
                                }
                        }
                    continue;
                }
                if (kind == Kind::ThrowSword) {
                    a.amount = distance(v.mode, u.position, c);
                    if (validate(v, a))
                        out.push_back(a);
                    continue;
                }
                if (kind == Kind::SwordStab) {
                    for (int include : {0, 1}) {
                        a.amount = include;
                        if (validate(v, a))
                            out.push_back(a);
                    }
                    continue;
                }
                if (validate(v, a))
                    out.push_back(a);
            }
        }
    return out;
}

bool hasAction(const View &v, int id) {
    auto actions = choices(v, id);
    return std::any_of(actions.begin(), actions.end(), [](const Action &a) {
        return a.kind != Kind::End && a.kind != Kind::Surrender;
    });
} // 普通行动和合法技能都用同一裁判规则判断。
