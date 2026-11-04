// Short own-turn plans keep the actual budget and resources. Only expected
// damage is projected: hidden hits, cooldown resets and kills are never assumed.
double botLegalAttack(const View &v, const BotKnowledge &knowledge, int id, int piece = 0) {
    const auto &u = v.units[id];
    int from = soldierCell(u, piece);
    double best = 0;
    auto value = [&](int cell, double power = 2, bool ranged = false, bool execute = false) {
        return botCellValue(v, knowledge, id, cell, power, ranged, execute);
    };
    for (int t = 1; t <= cellCount(v.mode); ++t) {
        Action a{id, Kind::Attack, t, -1, piece};
        if (validate(v, a))
            best = std::max(best, value(t, u.role == Role::Ink ? 1 : 2,
                                        u.role == Role::Sniper || u.role == Role::Ink ||
                                            (u.role == Role::HiddenWolf && !u.form),
                                        heroMode(v.mode) && u.role == Role::Sniper));
        for (Kind kind : {Kind::Thrust, Kind::Flame, Kind::SwordStab, Kind::Cleave}) {
            if ((kind == Kind::Thrust && u.role != Role::Armiger) ||
                (kind == Kind::Flame && u.role != Role::Flamer) ||
                (kind == Kind::SwordStab && u.role != Role::Sword) ||
                (kind == Kind::Cleave && u.role != Role::Knight))
                continue;
            a.kind = kind;
            a.amount = kind == Kind::Flame ? std::min(3, u.ammo) : 0;
            if (!validate(v, a))
                continue;
            double score = 0;
            if (kind == Kind::Thrust || kind == Kind::Flame)
                for (int c :
                     rayCells(v, from, t,
                              kind == Kind::Flame ? a.amount : 1 + int(std::ceil(u.armor - 1e-9))))
                    score += value(c, 2, kind == Kind::Flame);
            else if (kind == Kind::SwordStab)
                for (int c : swordLine(v, from, t, false))
                    score += value(c);
            else
                score = value(t);
            best = std::max(best, score);
        }
    }
    if (u.role == Role::Prince)
        for (const auto &rose : v.roses)
            if (rose.unit == id && rose.health > 0)
                for (int landing = 1; landing <= cellCount(v.mode); ++landing)
                    if (distance(v.mode, rose.cell, landing) <= 1 && canEnter(v, id, landing))
                        for (int target = 1; target <= cellCount(v.mode); ++target)
                            if (distance(v.mode, landing, target) <= 1 &&
                                validate(v,
                                         {id, Kind::Bloom, target, -1, 0, rose.cell, 1, landing}))
                                best = std::max(best, value(target));
    return best;
}

View botProject(const View &v, const Action &a, const BotKnowledge &knowledge) {
    View next = v;
    next.notices.clear();
    auto &u = next.units[a.unit];
    auto &b = next.budget;
    b.active = a.unit;
    if (a.kind == Kind::PlantRose) {
        int identity = 1;
        for (const auto &rose : next.roses)
            identity = std::max(identity, rose.id + 1);
        next.roses.push_back({a.unit, a.target, 3, identity});
        ++b.moves;
        ++b.used;
        botProjectStrike(next, knowledge, a.unit, a.target, 1);
    } else if (a.kind == Kind::RecallSword) {
        u.swordQi -= distance(v.mode, u.position, a.target);
        next.swords.erase(
            std::remove_if(next.swords.begin(), next.swords.end(),
                           [&](const Sword &s) { return s.unit == a.unit && s.cell == a.target; }),
            next.swords.end());
        ++b.extraAttacks;
    } else {
        if (a.target) {
            if (u.role == Role::WallGuard)
                u.soldiers[a.piece] = a.target;
            else
                u.position = a.kind == Kind::Dash ? dashPath(v, a.unit, a.target).back() : a.target;
        }
        if (a.kind == Kind::FireStep) {
            --u.ammo;
            b.fireStep = true;
            if (a.amount)
                botProjectStrike(next, knowledge, a.unit, a.target, 2);
        } else if (a.kind == Kind::Dash) {
            b.dashed = true;
            u.cooldown = 3;
            ++b.used;
        } else if (a.kind == Kind::FlameAdvance) {
            if (a.target)
                b.advance = true;
            b.flameChoice = false;
        } else if (a.kind == Kind::SwordAdvance)
            b.swordAdvance = false;
        else if (a.kind == Kind::Hunt) {
            ++b.used;
            b.phase = 3;
            b.anchor = a.target;
            botProjectStrike(next, knowledge, a.unit, a.target, 2);
        } else if (a.kind == Kind::Move || a.kind == Kind::Lead) {
            if (u.role == Role::Sword) {
                if (!b.swordBaseUsed && !b.moves && !b.attacks && !b.swordThrown)
                    b.swordBaseUsed = true;
                else
                    --b.extraAttacks;
            }
            ++b.moves;
            ++b.used;
            if (u.role == Role::WallGuard)
                b.squadDone |= 1 << a.piece;
            if (u.role == Role::HiddenWolf && u.form && b.attacks) {
                --b.moves;
                b.instinct = false;
            }
        }
    }
    return next;
}

double botContinuation(const View &v, const BotKnowledge &knowledge, int id, int piece = 0,
                       int depth = 1) {
    double best = botLegalAttack(v, knowledge, id, piece);
    if (depth <= 0)
        return best;
    // A second legal movement can unlock a bite/thrust without pretending that
    // Alpha, Sniper or Sword can attack after spending ordinary movement.
    const auto &u = v.units[id];
    if (u.role != Role::Wolf && u.role != Role::Ink && u.role != Role::Armiger)
        return best;
    int from = soldierCell(u, piece);
    for (int t = 1; t <= cellCount(v.mode); ++t)
        if (t != from && distance(v.mode, from, t) == 1) {
            Action a{id, Kind::Move, t, -1, piece};
            if (validate(v, a))
                best = std::max(best, 0.8 * botContinuation(botProject(v, a, knowledge), knowledge,
                                                            id, piece, depth - 1));
        }
    return best;
}
