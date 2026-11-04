// Profession emblems occupy the centre; public objects occupy the lower right.
COLORREF buildingTint(Structure kind) {
    return kind == Structure::GreatWall ? RGB(216, 235, 220)
           : kind == Structure::Fort    ? RGB(220, 230, 235)
                                        : RGB(236, 226, 210);
}

RECT centeredMarker(RECT cell, int index, int count, bool objects = false) {
    if (count <= 0)
        return cell;
    int width = cell.right - cell.left, height = cell.bottom - cell.top;
    int columns = std::min(count, count <= 2   ? 2
                                  : count <= 6 ? 3
                                               : 4),
        rowsCount = (count + columns - 1) / columns;
    // Coordinates have their own small header; names sit close to the foot of
    // the tile. Public objects retain a separate bottom-right strip.
    int header = width < 75 ? 13 : 17;
    int slotWidth = (width - 6) / columns,
        slotHeight = std::max(1, (height - header - (objects ? 29 : 4)) / rowsCount);
    int totalWidth = slotWidth * columns, startX = cell.left + (width - totalWidth) / 2,
        startY = cell.top + header;
    return rect(startX + index % columns * slotWidth, startY + index / columns * slotHeight,
                slotWidth, slotHeight);
}

std::wstring markerName(const Unit &unit, int piece) {
    return unit.role == Role::WallGuard ? L"士兵" + std::to_wstring(piece + 1)
                                        : roleName(unit.role);
}

void roleMarker(HDC dc, RECT slot, const Unit &unit, int piece, bool own, bool selected,
                int actorId = -1, bool walking = false, int horizontalDirection = 0,
                mist::presentation::sprites::Pose pose = {}) {
    int width = slot.right - slot.left, height = slot.bottom - slot.top;
    int font = std::max(
        7, std::min(12, std::min(width / int(markerName(unit, piece).size()), height / 4)));
    int labelHeight = std::min(height, font + 3), imageHeight = std::max(0, height - labelHeight);
    RECT icon = rect(slot.left + 1, slot.top, std::max(1, width - 2), imageHeight);
    // Sword's floating blades extend its union bounds well above the idle body.
    // Use the spare centre of the header on single-actor tiles, keeping one
    // stable scale for all its frames and the same feet/name baseline.
    if (unit.role == Role::Sword)
        icon.top -= std::clamp(std::min(width - 57, height - 54), 0, 8);
    if (imageHeight >= 8 && width >= 8) {
#ifdef _WIN32
        if (selected && pose.opacity > 0) {
            art::Canvas canvas(dc);
            auto &g = *canvas.graphics;
            float ringWidth = std::min(float(width - 4), imageHeight * .9f);
            float x = (slot.left + slot.right - ringWidth) / 2.f;
            art::ellipse(g, x, float(icon.bottom) - 6, ringWidth, 6,
                         Gdiplus::Color(45, 35, 142, 111));
            Gdiplus::Pen pen(Gdiplus::Color(185, 40, 150, 116), 1.3f);
            g.DrawEllipse(&pen, x, float(icon.bottom) - 6, ringWidth, 6.f);
        }
#endif
        if (!characterSprite(dc, icon, unit, actorId, walking, horizontalDirection, pose))
            emblem(dc, icon, unit.role, own, selected);
    }
    if (pose.opacity > .3 && labelHeight >= 7)
        text(dc, markerName(unit, piece),
             rect(slot.left, slot.top + imageHeight, width, labelHeight), font,
             selected ? GOLD
             : own    ? TEAL
                      : CORAL,
             true, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

std::vector<RECT> objectSlots(RECT cell, int count) {
    std::vector<RECT> result;
    int side = std::min(25, std::max(8, int(cell.right - cell.left - 8) / std::max(1, count)));
    for (int i = 0; i < count; ++i)
        result.push_back(
            rect(cell.right - 4 - (count - i) * side, cell.bottom - side - 4, side, side));
    return result;
}

void symbolStroke(HDC dc, const std::vector<POINT> &points) {
    if (points.empty())
        return;
    MoveToEx(dc, points[0].x, points[0].y, nullptr);
    for (std::size_t i = 1; i < points.size(); ++i)
        LineTo(dc, points[i].x, points[i].y);
}

bool paintedStructure(HDC dc, RECT slot, int frame, float opacity);

void drawObjectSymbol(HDC dc, RECT slot, int kind) {
    if (kind >= 2 && paintedStructure(dc, slot, kind == 2 ? 2 : kind == 3 ? 1 : 3, 1.f))
        return;
    drawArt(dc, slot, kind == 0 ? 13 : kind == 1 ? 14 : kind == 2 ? 16 : 15);
    if (kind == 4) {
        RECT tower = rect(slot.right - 9, slot.top, 9, slot.bottom - slot.top);
        drawArt(dc, tower, 16);
    }
}

std::vector<POINT> attackFrame(RECT r) {
    return {{r.left + 2, r.top + 2},
            {r.right - 2, r.top + 2},
            {r.right - 2, r.bottom - 2},
            {r.left + 2, r.bottom - 2},
            {r.left + 2, r.top + 2}};
}

int attackFrameWidth(bool active) {
    return active ? 2 : 1;
}

#include "terrain_art.inc"

void drawTerrain(HDC dc, RECT cell, Terrain terrain, ULONGLONG now, bool occupied) {
    if (terrain == Terrain::Lake || terrain == Terrain::Whirlpool)
        paintedTerrain(dc, cell, terrain, now, occupied);
    else if (terrain == Terrain::Wall) {
        int width = int(cell.right - cell.left), height = int(cell.bottom - cell.top);
        int header = s.page == Page::Config ? 2 : std::min(12, height / 7);
        int footer = s.page == Page::Config ? 2 : std::min(15, height / 6);
        int side = std::max(8, std::min(width - 4, height - header - footer));
        paintedStructure(dc, rect(cell.left + (width - side) / 2, cell.top + header, side, side),
                         0);
    }
}

ULONGLONG feedbackNow() {
    return playbackClock(visualNow());
}

#include "skill_fx_draw.inc"
