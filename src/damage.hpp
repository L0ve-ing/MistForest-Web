#pragma once
#include "game.hpp"

namespace mist::damage {
// Pure arithmetic for an eligible hit. Target selection, allegiance, visibility,
// turn transitions and feedback belong to the caller. Values use internal half-points.
struct Attack {
    double power = 2;
    bool ranged = false, execute = false, burning = false;
};

struct Hit {
    double prior = 0, strength = 0, delta = 0, ledgerAfter = 0;
};

struct ObjectResult {
    Hit hit;
    double healthAfter = 0;
};

struct UnitResult {
    Hit hit;
    double armorAfter = 0, shieldAfter = 0, healthAfter = 0, temporaryAfter = 0;
    double armorDamage = 0, shieldDamage = 0, bodyDamage = 0;
};

struct GuardSharing {
    bool shared = false;
    int count = 0;
    double each = 0;
    std::array<double, 3> healthAfter{};
};

ObjectResult againstBuilding(const Building &target, Role attacker, const Attack &attack);
ObjectResult againstRose(const Rose &target, const Attack &attack);
// piece must identify a living soldier for WallGuard, or be zero for other roles.
UnitResult againstUnit(const Unit &target, int piece, const Attack &attack);
// Requires the actual/fully known formation. AI must not invent unknown positions.
// Only direct, non-execution damage is eligible for sharing.
GuardSharing shareGuards(int width, const std::array<int, 3> &cells,
                        const std::array<double, 3> &health, int piece, double amount);
bool buildingProtectsUnits(const Building &building, bool execute);
} // namespace mist::damage
