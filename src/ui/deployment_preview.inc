// Render-only deployment data: never write a guessed position into the live game.
bool deploymentPreviewActor(int unit) {
    return s.page == Page::Battle && s.phase == Phase::Setup && unit == s.viewer * 2 + s.setupSlot;
}

bool deploymentActorSelected(int unit, int piece) {
    return deploymentPreviewActor(unit) &&
           (s.selectedRole != Role::WallGuard || piece == s.setupPiece);
}

std::array<Unit, 8> deploymentBoardUnits(Mode mode, const Map &map) {
    std::array<Unit, 8> result{};
    for (auto &unit : result)
        unit.health = 0;
    if (s.viewer < 0 || s.viewer >= 4)
        return result;
    const bool preview = s.page == Page::Battle && s.phase == Phase::Setup;
    for (int slot = 0; slot < teamCount(); ++slot) {
        const int id = s.viewer * 2 + slot;
        if (preview && slot > s.setupSlot)
            continue;
        const bool draft = preview && slot == s.setupSlot;
        auto &unit = result[id];
        unit.role = draft ? s.selectedRole : s.roles[id];
        if (unit.role == Role::WallGuard) {
            for (int piece = 0; piece < 3; ++piece) {
                const int cell = s.formations[id][piece];
                if (!valid(mode, cell, map))
                    continue;
                unit.soldiers[piece] = cell;
                unit.soldierHealth[piece] = 1.6;
                unit.health += unit.soldierHealth[piece];
            }
            unit.position = unit.soldiers[0];
        } else {
            const int cell = draft ? s.choice.state().selected : s.starts[id];
            if (!valid(mode, cell, map))
                continue;
            unit.position = cell;
            unit.health = initialHealth(unit.role, mode);
        }
    }
    return result;
}

void deploymentOutline(HDC dc, RECT cell) {
#ifdef _WIN32
    HPEN pen = CreatePen(PS_DASH, 1, TEAL);
    auto oldPen = SelectObject(dc, pen);
    auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, cell.left + 3, cell.top + 3, cell.right - 3, cell.bottom - 3);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
#endif
}
