int boardGap(Mode mode) {
    return (s.page == Page::Battle ? (s.tutorial ? 440 : 560) : 440) /
           std::max(size(mode), rows(mode));
} // 按最长边缩放长方形地图。

RECT cellRect(Mode mode, int cell) {
    int n = size(mode), gap = boardGap(mode),
        x = s.page == Page::Battle ? (s.tutorial ? 514 : 454) : 82,
        y = s.page == Page::Battle ? (s.tutorial ? 390 : 210) : 226;
    return rect(x + (cell - 1) % n * gap, y + (cell - 1) / n * gap, gap - 5, gap - 5);
} // 最多九行九列都在面板内。

RECT edgeRect(bool horizontal, int index) {
    Mode mode = mapMode(s.editor.document.map());
    int n = size(mode), gap = boardGap(mode), r = horizontal ? index / n : index / (n + 1),
        c = horizontal ? index % n : index % (n + 1);
    return horizontal ? rect(79 + c * gap, 223 + r * gap - 4, gap - 4, 8)
                      : rect(79 + c * gap - 4, 223 + r * gap, 8, gap - 4);
} // 可编辑边墙点击区域。

bool paintedLineWall(HDC dc, int x, int y, int length, bool horizontal);

void drawWalls(HDC dc, Mode mode, const Map &map, bool editing = false) {
    if (!heroMode(mode))
        return;
    int n = size(mode), gap = boardGap(mode);
    for (int dir = 0; dir < 2; ++dir) {
        bool horizontal = dir == 0;
        int count = horizontal ? (rows(mode) + 1) * n : rows(mode) * (n + 1);
        for (int i = 0; i < count; ++i) {
            bool wall = horizontal ? map.horizontal[i] : map.vertical[i];
            if (!wall && !editing)
                continue;
            int row = horizontal ? i / n : i / (n + 1), col = horizontal ? i % n : i % (n + 1);
            int x = (s.page == Page::Battle ? (s.tutorial ? 511 : 451) : 79) + col * gap,
                y = (s.page == Page::Battle ? (s.tutorial ? 387 : 207) : 223) + row * gap;
            if (!wall || !paintedLineWall(dc, x, y, gap, horizontal)) {
                HPEN pen = CreatePen(wall ? PS_SOLID : PS_DOT, wall ? 4 : 1,
                                     wall ? RGB(143, 80, 45) : RGB(161, 182, 170));
                auto old = SelectObject(dc, pen);
                MoveToEx(dc, x, y, nullptr);
                LineTo(dc, x + (horizontal ? gap : 0), y + (horizontal ? 0 : gap));
                SelectObject(dc, old);
                DeleteObject(pen);
            }
            if (editing && (s.editor.document.brush() == Brush::Line || s.editor.document.brush() == Brush::Erase))
                s.hits.push_back({edgeRect(horizontal, i),
                                  (horizontal ? HorizontalBase : VerticalBase) + i, true});
        }
    }
} // 绘图、文件和裁判使用相同边墙索引。

Action selectedAction(const View &v, int target) {
    return s.choice.action(v, target);
} // 参数选择不消耗行动，只有执行按钮才提交。

bool targetlessKind(Kind k) {
    return k == Kind::Follow || k == Kind::Transform || k == Kind::Silence || k == Kind::Blood ||
           k == Kind::ArmorUp || k == Kind::ArmorDown || k == Kind::ShadowMode;
} // 这些技能不允许任意指定地图目标。

#include "symbols.inc"
#include "board_feedback.inc"
#include "deployment_preview.inc"

void board(HDC dc, Mode mode, const Map &sourceMap, const std::array<Unit, 8> &sourceUnits,
           bool reveal = false) { // 只对真正可选的目标高亮，沉默用独立范围预览。
    if (s.page == Page::Battle && !s.tutorial) {
        RECT first = cellRect(mode, 1), last = cellRect(mode, size(mode) * rows(mode));
        RECT frame{first.left - 9, first.top - 9, last.right + 9, last.bottom + 9};
        card(dc, frame, RGB(219, 220, 191), RGB(143, 153, 114), 9, 2);
#ifdef _WIN32
        art::Canvas canvas(dc);
        auto &g = *canvas.graphics;
        for (int x = frame.left; x < frame.right - 4; x += 56) {
            float width = float(std::min(56, int(frame.right - x)));
            terrainArt::drawRegion(g, 5, Gdiplus::RectF(0, .43f, 1, .14f),
                                   Gdiplus::RectF(float(x), float(frame.top), width, 8), .9f, false,
                                   2253);
            terrainArt::drawRegion(g, 5, Gdiplus::RectF(0, .43f, 1, .14f),
                                   Gdiplus::RectF(float(x), float(frame.bottom - 8), width, 8), .9f,
                                   false, 2253);
        }
        for (int y = frame.top + 8; y < frame.bottom - 8; y += 56) {
            float height = float(std::min(56, int(frame.bottom - 8 - y)));
            for (int x : {frame.left, frame.right - 8})
                terrainArt::drawRegion(g, 6, Gdiplus::RectF(.43f, 0, .14f, 1),
                                       Gdiplus::RectF(float(x), float(y), 8, height), .9f, false,
                                       2253);
        }
        for (int x = frame.left + 8; x < frame.right - 16; x += 29) {
            art::line(g, float(x), float(frame.top + 1), float(x + 23), float(frame.top + 1),
                      Gdiplus::Color(215, 246, 239, 204), 3);
            art::line(g, float(x), float(frame.bottom - 2), float(x + 23), float(frame.bottom - 2),
                      Gdiplus::Color(160, 110, 133, 88), 3);
        }
        for (int x : {frame.left + 1, frame.right - 1}) {
            art::sprig(g, float(x), float(frame.top + 22), 30, x == frame.left + 1,
                       Gdiplus::Color(220, 111, 145, 73));
            art::sprig(g, float(x), float(frame.bottom - 5), 28, x == frame.left + 1,
                       Gdiplus::Color(200, 133, 156, 85));
        }
#endif
    }
    auto units = sourceUnits;
    bool playing = battleInputReady();
    View view;
    bool live = s.page == Page::Battle &&
                ((s.online && s.net && s.net->hasView()) || (!s.online && s.game));
    advanceReplay(visualNow());
    const auto *replayFrame = s.page == Page::Replay ? s.replayActors.displayedFrame() : nullptr;
    const Map &map = replayFrame ? replayFrame->map : sourceMap;
    if (s.page == Page::Replay)
        if (const auto *frame = s.replayActors.displayedFrame())
            units = frame->units;
    const auto movements = live && s.historyTurn < 0 ? s.playback.movements()
                           : s.page == Page::Replay && s.replayActors.busy()
                               ? s.replayMovements
                               : std::vector<mist::presentation::sprites::Movement>{};
    const auto &timeline = s.page == Page::Replay ? s.replayActors : s.playback;
    if ((live && s.historyTurn < 0) || s.page == Page::Replay)
        if (const auto *before = timeline.beforeFrame()) {
            // Keep the last KNOWN sprite until its defeat feedback has faded.
            for (int id = 0; id < 8; ++id) {
                if (!reveal && id / 2 != s.viewer)
                    continue;
                for (int piece = 0; piece < (units[id].role == Role::WallGuard ? 3 : 1); ++piece) {
                    auto pose = currentActorPose(id, piece);
                    if (pose.opacity <= 0)
                        continue;
                    const auto &old = before->units[id];
                    if (old.role == Role::WallGuard && old.soldierHealth[piece] > 0 &&
                        units[id].soldierHealth[piece] <= 0) {
                        units[id].health = std::max(1., units[id].health);
                        units[id].soldierHealth[piece] = old.soldierHealth[piece];
                        units[id].soldiers[piece] = old.soldiers[piece];
                    } else if (old.role != Role::WallGuard && old.health > 0 &&
                               units[id].health <= 0)
                        units[id] = old;
                }
            }
        }
    auto moving = [&](int unit, int piece) {
        return std::any_of(movements.begin(), movements.end(),
                           [&](const auto &m) { return m.unit == unit && m.piece == piece; });
    };
    s.actorSprites.beginFrame();
    if (live)
        view = displayedView();
    if (s.page == Page::Replay) {
        if (s.replayPrivate && !s.replayViews.empty())
            view = s.replayViews.at(s.replay);
        else if (s.last)
            view = s.last->historicalView(s.viewer, s.replay);
        if (const auto *frame = s.replayActors.displayedFrame())
            view = *frame;
    } // 缓存本人受限视图，不查询敌人真实位置。
    bool area = playing && s.choice.state().kind == Kind::Silence &&
                validate(view, {s.choice.state().unit, Kind::Silence}); // 范围只在技能可用时显示。
    forestBoardFrame(dc, cellRect(mode, 1), cellRect(mode, cellCount(mode)));
    for (int cell = 1; cell <= cellCount(mode); ++cell) {
        RECT r = cellRect(mode, cell);
        int cellSaved = SaveDC(dc);
        IntersectClipRect(dc, r.left, r.top, r.right, r.bottom);
        bool compact = boardGap(mode) < 80;
        Terrain terrain = map.cells[cell - 1];
        bool possible = false;
        if (playing && s.assist)
            for (int i = 0; i < 8; ++i)
                if (i / 2 != s.viewer && view.units[i].health > 0 && view.possible[i][cell - 1])
                    possible = true; // 可选推理辅助与技能范围相互独立。
        bool stabPreview = false;
        if (playing && s.choice.state().kind == Kind::SwordStab && s.choice.state().selected) {
            for (int c : swordLine(view, view.units[s.choice.state().unit].position,
                                   s.choice.state().selected, s.choice.state().swordIncludesSelf))
                if (c == cell)
                    stabPreview = true;
        }
        bool affected = area && silenceArea(mode, view.budget.anchor, cell, map);
        bool targetless = targetlessKind(s.choice.state().kind);
        bool enabled = s.page == Page::Editor ? s.editor.document.brush() != Brush::Line
                       : s.page == Page::Battle && s.phase == Phase::Setup
                           ? valid(mode, cell, map)
                           : false; // 自动范围技能不能任意指定格子。
        if (playing && !targetless) {
            Action a = selectedAction(view, cell);
            if (s.choice.state().kind == Kind::BuildWall &&
                view.units[s.choice.state().unit].role != Role::WallGuard) {
                if (s.choice.state().selected && !s.choice.state().extra) {
                    a.target = s.choice.state().selected;
                    a.extra = cell;
                    enabled = validate(view, a);
                } else {
                    for (int c = 1; c <= 81; ++c) {
                        a.extra = c;
                        if (validate(view, a)) {
                            enabled = true;
                            break;
                        }
                    }
                }
            } else if (s.choice.state().kind == Kind::Bloom && !s.choice.state().extra) {
                for (const auto &rose : view.roses)
                    if (rose.unit == s.choice.state().unit && rose.cell == cell &&
                        rose.health > 0 &&
                        std::find(view.budget.roseUsed.begin(), view.budget.roseUsed.end(),
                                  rose.id) == view.budget.roseUsed.end())
                        enabled = true;
            } else if (s.choice.state().kind == Kind::Bloom && !s.choice.state().bloomLanding) {
                enabled = canEnter(view, s.choice.state().unit, cell) &&
                          distance(view.mode, s.choice.state().extra, cell) <= 1;
            } else
                enabled = validate(view, a);
        } // 仅标注该动作的合法目标。
        bool buildTarget = playing && s.choice.state().kind == Kind::BuildWall &&
                           s.choice.state().selected == cell &&
                           view.units[s.choice.state().unit].role == Role::WallGuard;
        bool inPlan = std::any_of(s.choice.state().planned.begin(), s.choice.state().planned.end(),
                                  [&](const Action &a) { return a.target == cell; });
        bool chosen =
            (s.choice.state().selected == cell || s.choice.state().extra == cell ||
             (s.choice.state().kind == Kind::Bloom && s.choice.state().bloomLanding == cell) ||
             buildTarget || inPlan) &&
            !(playing && targetless);
        COLORREF color = terrain == Terrain::Wall        ? RGB(234, 239, 222)
                         : terrain == Terrain::Lake      ? RGB(237, 246, 240)
                         : terrain == Terrain::Whirlpool ? RGB(230, 243, 239)
                         : possible                      ? PALE
                                                         : WHITE;
        if (view.rubble[cell - 1])
            color = RGB(222, 219, 216);
        if (view.buildings[cell - 1].health)
            color = buildingTint(view.buildings[cell - 1].kind);
        if (view.fires[cell - 1].expires)
            color = RGB(255, 210, 168);
        if (stabPreview)
            color = RGB(255, 241, 204);
        else if (affected)
            color = RGB(238, 223, 250);
        else if (playing && enabled && terrain == Terrain::Ground &&
                 !view.buildings[cell - 1].health && !view.fires[cell - 1].expires)
            color = RGB(232, 244, 235); // 紫色表示沉默范围，绿色表示可操作目标。
        card(dc, r,
             (chosen || stabPreview) && !view.buildings[cell - 1].health ? RGB(255, 241, 204)
                                                                         : color,
             (chosen || stabPreview) ? GOLD
             : affected              ? RGB(145, 96, 165)
             : playing && enabled    ? TEAL
                                     : LINE,
             12, chosen || stabPreview || affected ? 3 : 1);
        bool occupied = false;
        for (int i = 0; i < 8; ++i)
            if (units[i].health > 0 && (reveal || i / 2 == s.viewer))
                for (int k = 0; k < (units[i].role == Role::WallGuard ? 3 : 1); ++k)
                    if (soldierCell(units[i], k) == cell &&
                        (units[i].role != Role::WallGuard || units[i].soldierHealth[k] > 0))
                        occupied = true;
        drawGroundDecoration(dc, r, terrain, cell, occupied, size(mode));
        drawTerrain(dc, r, terrain, visualNow(), occupied);
        if (view.buildings[cell - 1].health)
            paintedBuilding(dc, r, view.buildings[cell - 1].kind, occupied);
        else if (view.rubble[cell - 1])
            paintedStructure(dc,
                             rect(r.left + 5, r.top + 17, r.right - r.left - 10,
                                  std::max<int>(8, r.bottom - r.top - 34)),
                             4, occupied ? .55f : 1.f);
        text(dc, coordinate(mode, cell),
             rect(r.left + 4, r.top + 3, r.right - r.left - 8, compact ? 11 : 14), compact ? 8 : 10,
             MUTED, false, DT_LEFT | DT_SINGLELINE);
        bool publicObjects =
            view.buildings[cell - 1].health ||
            std::any_of(view.roses.begin(), view.roses.end(),
                        [&](const Rose &rose) { return rose.cell == cell && rose.health > 0; }) ||
            std::any_of(view.swords.begin(), view.swords.end(),
                        [&](const Sword &sword) { return sword.cell == cell; });
        if (terrain != Terrain::Ground && !occupied && !publicObjects)
            text(dc,
                 terrain == Terrain::Wall   ? L"方墙"
                 : terrain == Terrain::Lake ? L"湖泊"
                                            : L"漩涡",
                 rect(r.left + 3, r.bottom - (compact ? 15 : 19), r.right - r.left - 6,
                      compact ? 12 : 16),
                 compact ? 9 : 12, TEAL, true,
                 DT_CENTER | DT_SINGLELINE); // 名称在空格底部，不遮挡角色。
        if (view.rubble[cell - 1]) {
            text(dc, compact ? L"墟" : L"废墟",
                 rect(r.right - (compact ? 14 : 27), r.top + 3, compact ? 11 : 24, 12), 9, MUTED,
                 true);
        }
        if (view.buildings[cell - 1].health && !compact && !view.fires[cell - 1].expires &&
            !view.intel[cell - 1] && !view.rubble[cell - 1]) {
            text(dc,
                 (view.buildings[cell - 1].kind == Structure::Fort        ? L"堡垒"
                  : view.buildings[cell - 1].kind == Structure::GreatWall ? L"长城"
                                                                          : L"城墙") +
                     std::to_wstring(view.buildings[cell - 1].owner + 1),
                 rect(r.right - 35, r.top + 3, 31, 12), 9, INK, true);
        }
        bool hasRose = false;
        int swordCount = 0;
        for (const auto &rose : view.roses)
            if (rose.health > 0 && rose.cell == cell)
                hasRose = true;
        for (const auto &sword : view.swords)
            if (sword.cell == cell)
                ++swordCount;
        if (view.buildings[cell - 1].health && !occupied && !hasRose && !swordCount &&
            !view.fires[cell - 1].expires && !view.intel[cell - 1])
            text(dc,
                 view.buildings[cell - 1].kind == Structure::Fort        ? L"堡垒"
                 : view.buildings[cell - 1].kind == Structure::GreatWall ? L"长城"
                                                                         : L"城墙",
                 rect(r.left + 3, r.bottom - (compact ? 15 : 19), r.right - r.left - 6,
                      compact ? 12 : 16),
                 compact ? 9 : 12, TEAL, true, DT_CENTER | DT_SINGLELINE);
        int objectCount =
                int(view.buildings[cell - 1].health > 0) + int(hasRose) + int(swordCount > 0),
            objectIndex = 0;
        auto slots = objectSlots(r, objectCount);
        if (view.buildings[cell - 1].health) {
            if (occupied)
                drawObjectSymbol(dc, slots[objectIndex],
                                 view.buildings[cell - 1].kind == Structure::Fort   ? 2
                                 : view.buildings[cell - 1].kind == Structure::Wall ? 3
                                                                                    : 4);
            ++objectIndex;
        }
        RECT roseSlot{};
        if (hasRose) {
            roseSlot = slots[objectIndex];
            if (!growingRose(cell))
                drawObjectSymbol(dc, slots[objectIndex], 0);
            ++objectIndex;
        }
        if (swordCount > 0) {
            drawObjectSymbol(dc, slots[objectIndex++], 1);
            if (swordCount > 1 && !compact)
                text(dc, std::to_wstring(swordCount), rect(r.left + 4, r.bottom - 19, 20, 16), 11,
                     INK);
        }
        if (view.fires[cell - 1].expires)
            text(dc, compact ? L"火" : L"燃烧",
                 rect(r.right - (compact ? 14 : 27), r.top + 3, compact ? 11 : 24, 12), 9, CORAL,
                 true);
        else if (view.intel[cell - 1])
            text(dc, compact ? L"●" : L"瞬间位置",
                 rect(r.right - (compact ? 14 : 39), r.top + 3, compact ? 11 : 36, 12), 8, CORAL,
                 true); // 建筑与火焰公开，嗜血标记不显示敌人职业。
        int count = 0, totalMarkers = 0;
        for (int i = 0; i < 8; ++i)
            if (units[i].health > 0 && (reveal || i / 2 == s.viewer))
                for (int k = 0; k < (units[i].role == Role::WallGuard ? 3 : 1); ++k)
                    if (soldierCell(units[i], k) == cell &&
                        (units[i].role != Role::WallGuard || units[i].soldierHealth[k] > 0))
                        ++totalMarkers;
        if (view.fires[cell - 1].expires)
            fireEmbers(dc, r, visualNow());
        if (live || s.page == Page::Replay)
            attackSlash(dc, r, cell, hasRose ? &roseSlot : nullptr);
        for (int i = 0; i < 8; ++i)
            if (units[i].health > 0 && (reveal || i / 2 == s.viewer)) {
                const auto &u = units[i];
                for (int k = 0; k < (u.role == Role::WallGuard ? 3 : 1); ++k)
                    if (soldierCell(u, k) == cell &&
                        (u.role != Role::WallGuard || u.soldierHealth[k] > 0)) {
                        RECT dot = centeredMarker(r, count, totalMarkers, objectCount > 0);
                        if (!moving(i, k)) {
                            bool selected =
                                s.phase == Phase::Setup && s.page == Page::Battle
                                    ? deploymentActorSelected(i, k)
                                    : i == s.choice.state().unit && (u.role != Role::WallGuard ||
                                                                     k == s.choice.state().piece);
                            auto pose = deploymentPreviewActor(i)
                                            ? mist::presentation::sprites::Pose{}
                                            : currentActorPose(i, k);
                            if (deploymentPreviewActor(i)) {
                                pose.opacity = .58;
                                deploymentOutline(dc, r);
                            }
                            roleMarker(dc, dot, u, k, i / 2 == s.viewer, selected, i * 3 + k, false,
                                       0, pose);
                            if (live || s.page == Page::Replay)
                                actorStatus(dc, dot, view, i, k);
                        }
                        if (!(s.page == Page::Battle && s.phase == Phase::Setup) &&
                            i / 2 == s.viewer && i == s.choice.state().unit &&
                            (u.role != Role::WallGuard || k == s.choice.state().piece) &&
                            !moving(i, k))
                            selectionCorners(dc, r);
                        ++count;
                    }
            } // 三士兵各自显示生命位置，敌人仍保持隐藏。

        if (live || s.page == Page::Replay)
            impactAboveActor(dc, r, cell);
        bool replayAttack = s.page == Page::Replay && s.replay > 0 &&
                            (s.replayActors.busy() ? s.replayActors.state().active[cell - 1]
                                                   : s.replayAttacks[cell - 1]);
        if ((live &&
             (s.playback.state().enemySeen[cell - 1] || s.playback.state().active[cell - 1])) ||
            replayAttack) {
            bool now =
                replayAttack || (s.playback.state().running && s.playback.state().active[cell - 1]);
            COLORREF attackColor = RGB(205, 65, 50);
            HPEN attackPen = CreatePen(PS_SOLID, attackFrameWidth(now), attackColor);
            auto oldPen = SelectObject(dc, attackPen);
            auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            symbolStroke(dc, attackFrame(r));
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(attackPen);
        } else if (buildTarget && !compact && !occupied && objectCount == 0 &&
                   terrain == Terrain::Ground)
            text(dc, L"建造长城", rect(r.left + 3, r.bottom - 19, r.right - r.left - 6, 16), 11,
                 CORAL, true, DT_CENTER | DT_SINGLELINE);
        else if (affected && !compact && !occupied && objectCount == 0 &&
                 terrain == Terrain::Ground)
            text(dc, L"沉默范围", rect(r.left + 3, r.bottom - 19, r.right - r.left - 6, 16), 11,
                 RGB(115, 58, 138), true, DT_CENTER | DT_SINGLELINE);
        RestoreDC(dc, cellSaved);
        s.hits.push_back({r, CellBase + cell, enabled}); // 不再将全图显示为可沉默。
    }
    drawWalls(dc, mode, map, s.page == Page::Editor); // 墙绘制在格子之上。
    routePreview(dc, mode);
    // Moving actors are drawn once over the board, not once in each clipped cell.
    // The path endpoints come exclusively from this viewer's committed playback frames.
    int saved = SaveDC(dc);
    RECT first = cellRect(mode, 1), last = cellRect(mode, cellCount(mode));
    IntersectClipRect(dc, first.left, first.top, last.right, last.bottom);
    double progress = timeline.movementProgress(playbackClock(visualNow()));
    for (const auto &m : movements) {
        const auto &u = units[m.unit];
        if (u.health <= 0 || (!reveal && m.unit / 2 != s.viewer))
            continue;
        auto sample = mist::presentation::sprites::sampleMovement(m, progress);
        auto placement = [&](int cell, const View *frame) {
            int count = 0, index = 0;
            const auto &group = frame ? frame->units : units;
            for (int id = 0; id < 8; ++id)
                if (group[id].health > 0 && (reveal || id / 2 == s.viewer))
                    for (int k = 0; k < (group[id].role == Role::WallGuard ? 3 : 1); ++k)
                        if (soldierCell(group[id], k) == cell &&
                            (group[id].role != Role::WallGuard || group[id].soldierHealth[k] > 0)) {
                            if (id == m.unit && k == m.piece)
                                index = count;
                            ++count;
                        }
            bool objects =
                view.buildings[cell - 1].health > 0 ||
                std::any_of(view.roses.begin(), view.roses.end(),
                            [&](const Rose &r) { return r.cell == cell && r.health > 0; }) ||
                std::any_of(view.swords.begin(), view.swords.end(),
                            [&](const Sword &x) { return x.cell == cell; });
            return centeredMarker(cellRect(mode, cell), index, std::max(1, count), objects);
        };
        RECT origin = placement(m.from, timeline.beforeFrame()),
             destination = placement(m.to, timeline.afterFrame());
        RECT originCell = cellRect(mode, m.from), targetCell = cellRect(mode, m.to);
        double eased = progress * progress * (3 - 2 * progress);
        int left = int(std::lround(first.left + sample.column * boardGap(mode) +
                                   (origin.left - originCell.left) * (1 - eased) +
                                   (destination.left - targetCell.left) * eased));
        int top = int(std::lround(first.top + sample.row * boardGap(mode) +
                                  (origin.top - originCell.top) * (1 - eased) +
                                  (destination.top - targetCell.top) * eased -
                                  (s.fullAnimation ? sample.lift * boardGap(mode) : 0)));
        int width = int(std::lround((origin.right - origin.left) * (1 - eased) +
                                    (destination.right - destination.left) * eased));
        int height = int(std::lround((origin.bottom - origin.top) * (1 - eased) +
                                     (destination.bottom - destination.top) * eased));
        auto pose = currentActorPose(m.unit, m.piece);
        pose.opacity *= sample.opacity;
        // Translate copies by the grid's full period and clip to the board. Only
        // the two edge fragments of ONE actor are visible during a wrap.
        for (int sx : (u.role == Role::Shadow ? std::vector<int>{-size(mode), 0, size(mode)}
                                              : std::vector<int>{0}))
            for (int sy : (u.role == Role::Shadow ? std::vector<int>{-rows(mode), 0, rows(mode)}
                                                  : std::vector<int>{0})) {
                RECT slot =
                    rect(left + sx * boardGap(mode), top + sy * boardGap(mode), width, height);
                if (slot.right <= first.left || slot.left >= last.right ||
                    slot.bottom <= first.top || slot.top >= last.bottom)
                    continue;
                roleMarker(dc, slot, u, m.piece, m.unit / 2 == s.viewer,
                           m.unit == s.choice.state().unit &&
                               (u.role != Role::WallGuard || m.piece == s.choice.state().piece),
                           m.unit * 3 + m.piece, progress < 1, sample.facing, pose);
                if (live || s.page == Page::Replay)
                    actorStatus(dc, slot, view, m.unit, m.piece);
            }
    }
    RestoreDC(dc, saved);
    s.actorSprites.endFrame();
} // 棋盘结束。
