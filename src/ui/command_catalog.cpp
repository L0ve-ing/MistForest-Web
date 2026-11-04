#include "command_catalog.hpp"

namespace ui {
namespace {
struct Binding { int id; mist::Kind kind; int key; };
constexpr Binding Bindings[] = {
    {Attack, mist::Kind::Attack, 'E'},
    {Move, mist::Kind::Move, 'W'},
    {Dash, mist::Kind::Dash, 'R'},
    {Follow, mist::Kind::Follow, 'R'},
    {Lead, mist::Kind::Lead, 'T'},
    {Charge, mist::Kind::Charge, 'E'},
    {Fort, mist::Kind::BuildFort, 'Z'},
    {WallBuild, mist::Kind::BuildWall, 'C'},
    {Blood, mist::Kind::Blood, 'R'},
    {ArmorUp, mist::Kind::ArmorUp, 'R'},
    {ArmorDown, mist::Kind::ArmorDown, 'T'},
    {Thrust, mist::Kind::Thrust, 'E'},
    {Flame, mist::Kind::Flame, 'E'},
    {FireStep, mist::Kind::FireStep, 'R'},
    {FlameAdvance, mist::Kind::FlameAdvance, 'T'},
    {Transform, mist::Kind::Transform, 'R'},
    {Leap, mist::Kind::Leap, 'W'},
    {Hunt, mist::Kind::Hunt, 'E'},
    {Silence, mist::Kind::Silence, 'E'},
    {Cleave, mist::Kind::Cleave, 'R'},
    {Retreat, mist::Kind::Retreat, 'T'},
    {Row, mist::Kind::Row, 'R'},
    {Col, mist::Kind::Col, 'T'},
    {PlantRose, mist::Kind::PlantRose, 'E'},
    {Bloom, mist::Kind::Bloom, 'R'},
    {SwordStab, mist::Kind::SwordStab, 'E'},
    {ThrowSword, mist::Kind::ThrowSword, 'R'},
    {RecallSword, mist::Kind::RecallSword, 'T'},
    {SwordAdvance, mist::Kind::SwordAdvance, 'Y'},
    {Demolish, mist::Kind::Demolish, 'X'},
};
}

bool commandKind(int id, mist::Kind &kind) {
    for (const auto &binding : Bindings)
        if (binding.id == id) {
            kind = binding.kind;
            return true;
        }
    return false;
}

int shortcut(int id) {
    for (const auto &binding : Bindings)
        if (binding.id == id)
            return binding.key;
    switch (id) {
    case LogPrevious:
        return 'A';
    case LogNext:
        return 'D';
    case LogCurrent:
        return 'F';
    case CycleRole:
        return 'Q';
    case ShadowLeadMode:
        return 'R';
    case ShadowFollowMode:
    case RushToggle:
        return 'T';
    case Lunge:
    case IgniteToggle:
        return 'Y';
    case IncludeSelf:
        return 'U';
    case EndTurn:
    case StayAfterAttack:
        return 'S';
    case SelfSkills:
        return 'H';
    case Piece0:
    case Amount1:
    case SetupPiece0:
        return '1';
    case Piece1:
    case Amount2:
    case SetupPiece1:
        return '2';
    case Piece2:
    case Amount3:
    case SetupPiece2:
        return '3';
    case BatchToggle:
        return 'B';
    case BatchClear:
        return 'V';
    case EnemyPiece:
        return 'G';
    case CostDown:
        return '4';
    case CostUp:
        return '5';
    default:
        return 0;
    }
}
} // namespace ui
