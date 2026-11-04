#include "damage.hpp"
#include <algorithm>
#include <cmath>

namespace mist {
double directAttackDelta(double prior, double power) {
    return std::max(0.0, power - prior);
}

namespace damage {
namespace {
Hit incoming(double prior, const Attack &attack, bool vulnerable = false,
             double multiplier = 1) {
    double strength = attack.execute ? 1e6 : attack.power * multiplier + (vulnerable ? 1 : 0);
    return {prior, strength,
            attack.burning || attack.execute ? strength : directAttackDelta(prior, strength),
            attack.burning ? prior : std::max(prior, strength)};
}
} // namespace

bool buildingProtectsUnits(const Building &building, bool execute) {
    return building.health > 0 && !execute;
}

ObjectResult againstBuilding(const Building &target, Role attacker, const Attack &attack) {
    if (attacker == Role::Wolf && !attack.burning)
        return {{target.directDamage, 0, 0, target.directDamage}, target.health};
    auto hit = incoming(target.directDamage, attack, false,
                        attacker == Role::Ink && !attack.burning ? 2 : 1);
    return {hit, attack.execute ? 0 : std::max(0.0, target.health - hit.delta)};
}

ObjectResult againstRose(const Rose &target, const Attack &attack) {
    auto hit = incoming(target.directDamage, attack);
    return {hit, attack.execute ? 0 : std::max(0.0, target.health - hit.delta)};
}

UnitResult againstUnit(const Unit &target, int piece, const Attack &attack) {
    bool soldier = target.role == Role::WallGuard;
    double hp = soldier ? target.soldierHealth[piece] : target.health;
    double prior = soldier ? target.soldierDirectDamage[piece] : target.directDamage;
    bool vulnerable = soldier ? target.soldierVulnerable[piece] : target.vulnerable;
    UnitResult result;
    result.hit = incoming(prior, attack, !attack.execute && vulnerable);
    result.armorAfter = target.armor;
    result.shieldAfter = target.shield;
    result.healthAfter = hp;
    result.temporaryAfter = target.temporaryHealth;
    if (result.hit.delta <= 1e-9)
        return result;
    if (attack.execute) {
        result.armorAfter = result.shieldAfter = result.healthAfter = result.temporaryAfter = 0;
        result.bodyDamage = hp;
        return result;
    }
    double amount = result.hit.delta;
    if (!attack.burning && target.role == Role::HiddenWolf &&
        ((!target.form && attack.ranged) || (target.form && !attack.ranged)))
        amount /= 2;
    result.armorDamage = std::min(amount, target.armor * 2);
    result.armorAfter -= result.armorDamage / 2;
    amount -= result.armorDamage;
    result.shieldDamage = std::min(amount, target.shield);
    result.shieldAfter -= result.shieldDamage;
    result.bodyDamage = amount - result.shieldDamage;
    result.healthAfter = std::max(0.0, hp - result.bodyDamage);
    if (result.healthAfter < 1e-9)
        result.healthAfter = 0;
    if (!soldier)
        result.temporaryAfter = std::max(0.0, target.temporaryHealth - result.bodyDamage);
    return result;
}

GuardSharing shareGuards(int width, const std::array<int, 3> &cells,
                        const std::array<double, 3> &health, int piece, double amount) {
    GuardSharing result;
    result.healthAfter = health;
    if (amount <= 0)
        return result;
    int mask = 1 << piece, n = width;
    for (int pass = 0; pass < 3; ++pass)
        for (int i = 0; i < 3; ++i)
            if (mask & (1 << i))
                for (int j = 0; j < 3; ++j)
                    if (health[j] > 0 &&
                        std::abs((cells[i] - 1) / n - (cells[j] - 1) / n) <= 1 &&
                        std::abs((cells[i] - 1) % n - (cells[j] - 1) % n) <= 1)
                        mask |= 1 << j;
    for (int i = 0; i < 3; ++i)
        if (mask & (1 << i))
            ++result.count;
    result.each = amount / result.count;
    int deaths = 0;
    for (int i = 0; i < 3; ++i)
        if (mask & (1 << i))
            deaths += health[i] <= result.each + 1e-9;
    if (result.count > 1 && deaths <= int(health[piece] <= amount + 1e-9)) {
        result.shared = true;
        for (int i = 0; i < 3; ++i)
            if (mask & (1 << i))
                result.healthAfter[i] = std::max(0.0, health[i] - result.each);
    }
    return result;
}
} // namespace damage
} // namespace mist
