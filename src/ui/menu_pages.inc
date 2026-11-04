bool presentationBusy();
void drawTerrain(HDC dc, RECT cell, Terrain terrain, ULONGLONG now, bool occupied);
void drawGroundDecoration(HDC dc, RECT cellRect, Terrain terrain, int cell, bool occupied,
                          int columns);

void configurationAssistSwitch(HDC dc, RECT r) {
    bool hover = visual.hover == Assist;
#ifdef _WIN32
    {
        art::Canvas canvas(dc);
        auto &g = *canvas.graphics;
        RECT track = rect(r.left + 1, r.top + 3, 60, 25);
        Gdiplus::GraphicsPath path;
        roundedPanelPath(path, track, 12.5f);
        Gdiplus::SolidBrush paint(
            art::color(s.assist ? (hover ? RGB(51, 132, 93) : TEAL)
                                : (hover ? RGB(180, 187, 164) : RGB(191, 195, 177))));
        Gdiplus::Pen edge(art::color(s.assist ? RGB(26, 97, 70) : RGB(142, 152, 129)), 1);
        g.FillPath(&paint, &path);
        g.DrawPath(&edge, &path);
        float x = float(track.left + (s.assist ? 36 : 3));
        art::ellipse(g, x + 1, track.top + 3.f, 19, 19, Gdiplus::Color(45, 40, 47, 31));
        art::ellipse(g, x, track.top + 2.f, 20, 20, Gdiplus::Color(255, 255, 253, 238));
    }
#else
    card(dc, rect(r.left, r.top + 3, 60, 25), s.assist ? PALE : LINE, MUTED, 24);
#endif
    text(dc, s.assist ? L"开" : L"关", rect(r.left + 70, r.top + 5, 20, 22), 16, INK);
    s.hits.push_back({r, Assist, true});
}

void configurationPracticeIcon(HDC dc, RECT slot, bool role) {
#ifdef _WIN32
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    auto state = g.Save();
    g.TranslateTransform(float(slot.left), float(slot.top));
    g.ScaleTransform((slot.right - slot.left) / 32.f, (slot.bottom - slot.top) / 32.f);
    if (role) {
        Gdiplus::GraphicsPath helmet;
        helmet.AddBezier(6.f, 20.f, 2.f, -2.f, 30.f, -2.f, 26.f, 20.f);
        helmet.AddLine(26.f, 20.f, 22.f, 27.f);
        helmet.AddLine(22.f, 27.f, 16.f, 31.f);
        helmet.AddLine(16.f, 31.f, 10.f, 27.f);
        helmet.CloseFigure();
        Gdiplus::SolidBrush fill(art::color(TEAL));
        g.FillPath(&fill, &helmet);
        art::line(g, 16, 7, 16, 29, Gdiplus::Color(255, 221, 231, 197), 1.1f);
        art::line(g, 8, 16, 13, 18, Gdiplus::Color(255, 239, 240, 207), 2);
        art::line(g, 19, 18, 24, 16, Gdiplus::Color(255, 239, 240, 207), 2);
    } else {
        Gdiplus::PointF left[] = {{3, 4}, {12, 4}, {16, 7}, {16, 30}, {12, 27}, {3, 27}},
                        right[] = {{17, 7}, {21, 4}, {30, 4}, {30, 27}, {21, 27}, {17, 30}};
        Gdiplus::SolidBrush cover(art::color(TEAL));
        g.FillPolygon(&cover, left, 6);
        g.FillPolygon(&cover, right, 6);
        for (int y : {10, 15, 20}) {
            art::line(g, 6, float(y), 12, float(y), Gdiplus::Color(230, 226, 235, 199));
            art::line(g, 21, float(y), 27, float(y), Gdiplus::Color(230, 226, 235, 199));
        }
        art::line(g, 16.5f, 7, 16.5f, 29, Gdiplus::Color(240, 211, 222, 183));
    }
    g.Restore(state);
#endif
}

void configurationModeCard(HDC dc, int id, RECT r, bool selected, bool scout) {
    bool hover = visual.hover == id;
    card(dc, r,
         selected ? (hover ? RGB(214, 232, 201) : RGB(227, 237, 211))
                  : (hover ? RGB(247, 245, 226) : RGB(253, 249, 234)),
         selected ? TEAL : RGB(207, 195, 157), 14, selected ? 2 : 1);
#ifdef _WIN32
    {
        art::Canvas canvas(dc);
        auto &g = *canvas.graphics;
        float cx = (r.left + r.right) / 2.f, cy = r.top + 49.f;
        if (scout) {
            for (int y = 0; y < 3; ++y)
                for (int x = 0; x < 3; ++x) {
                    Gdiplus::SolidBrush square(Gdiplus::Color(220, 158 + x * 4, 166 + y * 4, 123));
                    g.FillRectangle(&square, cx - 34 + x * 24, cy - 34 + y * 24, 21.f, 21.f);
                }
            art::ellipse(g, cx - 11, cy - 6, 22, 13, art::color(TEAL));
            art::ellipse(g, cx - 5, cy - 4, 10, 9, Gdiplus::Color(255, 248, 248, 219));
            art::ellipse(g, cx - 2, cy - 2, 4, 5, art::color(TEAL));
        } else {
            for (float angle : {-43.f, 43.f}) {
                auto state = g.Save();
                g.TranslateTransform(cx, cy);
                g.RotateTransform(angle);
                Gdiplus::PointF blade[] = {{-3, -23}, {0, -37}, {3, -23}, {3, 11}, {-3, 11}};
                Gdiplus::SolidBrush steel(art::color(RGB(52, 104, 76)));
                g.FillPolygon(&steel, blade, 5);
                art::line(g, -10, 12, 10, 12, art::color(RGB(105, 122, 77)), 4);
                art::line(g, 0, 14, 0, 29, art::color(RGB(52, 104, 76)), 5);
                art::ellipse(g, -4, 27, 8, 8, art::color(RGB(105, 122, 77)));
                g.Restore(state);
            }
        }
        art::sprig(g, cx - 45, cy + 30, 32, true, Gdiplus::Color(140, 125, 153, 100));
        art::sprig(g, cx + 45, cy + 30, 32, false, Gdiplus::Color(140, 125, 153, 100));
        if (selected) {
            float x = float(r.right - 25), y = float(r.top + 23);
            art::ellipse(g, x - 12, y - 12, 24, 24, art::color(TEAL));
            art::line(g, x - 6, y, x - 1, y + 5, Gdiplus::Color(255, 252, 249, 222), 2);
            art::line(g, x - 1, y + 5, x + 7, y - 5, Gdiplus::Color(255, 252, 249, 222), 2);
        }
    }
#endif
    text(dc, scout ? L"3×3 无限侦察" : L"职业对战",
         rect(r.left + 20, r.top + 94, r.right - r.left - 40, 32), 23, INK, true,
         DT_CENTER | DT_SINGLELINE);
    text(dc, scout ? L"侦察不限次数" : L"十三职业 · 隐藏站位",
         rect(r.left + 20, r.top + 132, r.right - r.left - 40, 22), 16, MUTED, false,
         DT_CENTER | DT_SINGLELINE);
    s.hits.push_back({r, id, true});
}

void configurationMapPreview(HDC dc, RECT bounds, const Map &map) {
    int gap = std::max(1, int(std::min((bounds.right - bounds.left) / map.width,
                                       (bounds.bottom - bounds.top) / map.height)));
    int x = (bounds.left + bounds.right - map.width * gap) / 2,
        y = (bounds.top + bounds.bottom - map.height * gap) / 2;
    int saved = SaveDC(dc);
    IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    for (int row = 0; row < map.height; ++row)
        for (int column = 0; column < map.width; ++column) {
            int cell = row * map.width + column + 1;
            auto terrain = map.cells[cell - 1];
            RECT tile = rect(x + column * gap + 1, y + row * gap + 1, gap - 2, gap - 2);
            card(dc, tile,
                 terrain == Terrain::Lake || terrain == Terrain::Whirlpool ? RGB(229, 242, 231)
                                                                           : RGB(248, 246, 224),
                 RGB(156, 177, 137), 3);
            drawGroundDecoration(dc, tile, terrain, cell, false, map.width);
            drawTerrain(dc, tile, terrain, visualNow(), false);
        }
    HPEN pen = CreatePen(PS_SOLID, std::max(2, gap / 14), RGB(121, 117, 83));
    auto old = SelectObject(dc, pen);
    for (int row = 0; row <= map.height; ++row)
        for (int column = 0; column < map.width; ++column)
            if (map.horizontal[row * map.width + column]) {
                MoveToEx(dc, x + column * gap, y + row * gap, nullptr);
                LineTo(dc, x + (column + 1) * gap, y + row * gap);
            }
    for (int row = 0; row < map.height; ++row)
        for (int column = 0; column <= map.width; ++column)
            if (map.vertical[row * (map.width + 1) + column]) {
                MoveToEx(dc, x + column * gap, y + row * gap, nullptr);
                LineTo(dc, x + column * gap, y + (row + 1) * gap);
            }
    SelectObject(dc, old);
    DeleteObject(pen);
    RestoreDC(dc, saved);
}

void header(HDC dc) {
    if (s.page == Page::Lobby)
        return;
#ifdef _WIN32
    {
        art::Canvas canvas(dc);
        Gdiplus::LinearGradientBrush tint(Gdiplus::Rect(0, 0, canvasWidth(), 76),
                                          Gdiplus::Color(222, 66, 99, 69),
                                          Gdiplus::Color(237, 34, 70, 53), 90.f);
        canvas.graphics->FillRectangle(&tint, 0, 0, canvasWidth(), 76);
        art::line(*canvas.graphics, 0, 75, float(canvasWidth()), 75, art::color(GOLD));
        art::sprig(*canvas.graphics, float(canvasWidth() - 16), 61, 32, true,
                   Gdiplus::Color(155, 194, 204, 145));
    }
#else
    fill(dc, rect(0, 0, canvasWidth(), 76), TEAL);
#endif
    forestCrest(dc, rect(34, 12, 49, 49));
    text(dc, L"雾隐之森", rect(100, 8, 320, 36), 27, RGB(251, 247, 222), true);
    text(dc, L"V" + std::wstring(VersionText) + L" · 十三职业 · 单 / 双角色",
         rect(101, 47, 390, 20), 12, RGB(174, 202, 180));
    if (s.page == Page::Battle && canvasWidth() >= 1400) {
        card(dc, rect(490, 15, 345, 43), RGB(56, 89, 62), GOLD, 16);
        std::wstring label = s.phase == Phase::Cover      ? L"秘密视角交接"
                             : s.phase == Phase::Setup    ? L"布置阵容"
                             : s.phase == Phase::Finished ? L"对局结束"
                             : presentationBusy()         ? L"战场反馈播放中"
                             : s.phase == Phase::Playing  ? L"己方行动"
                                                          : L"等待其他玩家";
        text(dc, label, rect(503, 24, 319, 26), 19, RGB(250, 245, 220), true,
             DT_CENTER | DT_SINGLELINE);
    }
    bool back = s.page == Page::Skills || s.page == Page::Editor || s.page == Page::Network;
    button(dc, PlaybackOptions, L"播放设置", rect(canvasWidth() - 600, 17, 120, 40));
    button(dc, back ? Settings : Home, back ? L"返回设置" : L"返回大厅",
           rect(canvasWidth() - 466, 17, 132, 40));
    button(dc, s.page == Page::Battle ? SelfSkills : Skills,
           s.page == Page::Battle ? L"己方技能" : L"角色技能",
           rect(canvasWidth() - 321, 17, 132, 40), s.page == Page::Skills,
           s.page != Page::Battle ||
               ((s.online && s.net && s.net->hasView()) || (!s.online && s.game)));
    button(
        dc, Review, L"对局复盘", rect(canvasWidth() - 176, 17, 150, 40), s.page == Page::Replay,
        (s.last != nullptr || (s.online && s.net && s.net->phase() == forest::Phase::Recovering)));
} // 导航不再显示教学或四格模式。

void footer(HDC dc, const std::wstring &label) {
    if (widePage() || s.page == Page::Battle) {
#ifdef _WIN32
        {
            art::Canvas canvas(dc);
            Gdiplus::GraphicsPath path;
            roundedPanelPath(path,
                             rect(28, canvasHeight() - 35,
                                  std::min(canvasWidth() - 56, int(label.size()) * 13 + 24), 26),
                             7);
            Gdiplus::SolidBrush backdrop(Gdiplus::Color(166, 17, 42, 31));
            canvas.graphics->FillPath(&backdrop, &path);
        }
#endif
        text(dc, label, rect(39, canvasHeight() - 30, canvasWidth() - 76, 25), 13, RGB(12, 30, 21));
        text(dc, label, rect(38, canvasHeight() - 31, canvasWidth() - 76, 25), 13,
             RGB(248, 243, 215));
    } else
        text(dc, label, rect(38, canvasHeight() - 31, canvasWidth() - 76, 25), 13, MUTED);
} // 状态提示位于放大结果区域下方。

void lobby(HDC dc) {
    int cx = canvasWidth() / 2, dy = std::max(0, (canvasHeight() - 930) / 2);
    forestCrest(dc, rect(cx - 48, 82 + dy, 96, 96));
    text(dc, L"雾 隐 之 森", rect(cx - 390, 204 + dy, 780, 80), 62, RGB(251, 246, 215), true,
         DT_CENTER | DT_SINGLELINE);
    text(dc, L"M I S T F O R E S T", rect(cx - 380, 297 + dy, 760, 30), 17, RGB(195, 210, 175),
         false, DT_CENTER | DT_SINGLELINE);
    text(dc, L"隐秘站位 · 逐步交锋", rect(cx - 380, 347 + dy, 760, 40), 24, RGB(206, 220, 186),
         false, DT_CENTER | DT_SINGLELINE);
    card(dc, rect(cx - 320, 424 + dy, 640, 354), RGB(253, 249, 233), RGB(187, 166, 104), 24, 2);
    text(dc, L"走进迷雾，选择你的对手", rect(cx - 286, 451 + dy, 572, 29), 17, MUTED, false,
         DT_CENTER | DT_SINGLELINE);
    button(dc, Cpu, L"人机教学", rect(cx - 286, 500 + dy, 572, 84), true, true, 29);
    button(dc, Hotseat, L"基础版", rect(cx - 286, 606 + dy, 278, 69), false, true, 25);
    button(dc, Online, L"联机", rect(cx + 8, 606 + dy, 278, 69), false, true, 25);
    button(dc, Skills, L"角色技能", rect(cx - 262, 709 + dy, 158, 35), false, true, 16);
    button(dc, Review, L"对局复盘", rect(cx - 79, 709 + dy, 158, 35), false, s.last != nullptr, 16);
    button(dc, Settings, L"设置", rect(cx + 104, 709 + dy, 158, 35), false, true, 16);
    text(dc, L"十三职业 · 单 / 双角色 · 隐藏信息对战", rect(cx - 380, 821 + dy, 760, 30), 17,
         RGB(186, 206, 166), false, DT_CENTER | DT_SINGLELINE);
    text(dc, L"V" + std::wstring(VersionText), rect(cx - 380, 873 + dy, 760, 24), 14,
         RGB(170, 193, 159), false, DT_CENTER | DT_SINGLELINE);
}

void configuration(HDC dc) {
    const int margin = 48, gutter = 20, total = canvasWidth() - margin * 2,
              leftWidth = (total - gutter) * 59 / 100, rightX = margin + leftWidth + gutter,
              rightWidth = total - leftWidth - gutter, bottomY = canvasHeight() - 128, panelY = 194,
              panelHeight = bottomY - panelY - 16;
    const int lx = margin + 26, inner = leftWidth - 52, optionsX = margin + 180,
              optionsWidth = leftWidth - 206, rx = rightX + 26, rightInner = rightWidth - 52;
    const bool teaching = s.bot && !s.networkMode, heroes = heroMode(s.mode);
    const Map map = selectedMap();

    text(dc, L"对局设置", rect(margin + 8, 112, 240, 42), 34, RGB(251, 247, 222), true);
    card(dc, rect(margin + 231, 116, 136, 32), RGB(224, 235, 201), RGB(192, 207, 159), 22);
    text(dc,
         s.networkMode ? L"联机"
         : teaching    ? L"人机教学"
                       : L"本地同机",
         rect(margin + 239, 121, 120, 24), 17, TEAL, true, DT_CENTER | DT_SINGLELINE);
    text(dc,
         s.networkMode ? L"由房主设置地图与人数，再创建或加入房间。"
         : teaching    ? L"你操控玩家1，其他玩家由电脑操作。"
                       : L"各玩家依次布阵，交接时隐藏站位。",
         rect(margin + 8, 160, total - 16, 26), 17, RGB(243, 244, 217));

    card(dc, rect(margin, panelY, leftWidth, panelHeight), RGB(253, 250, 235), RGB(194, 184, 142),
         20);
    text(dc, L"选择对战模式", rect(lx, 213, inner, 30), 24, INK, true);
    int modeWidth = (inner - 18) / 2;
    configurationModeCard(dc, Heroes, rect(lx, 254, modeWidth, 160), heroes, false);
    configurationModeCard(dc, Small, rect(lx + modeWidth + 18, 254, modeWidth, 160), !heroes, true);
    fill(dc, rect(lx, 434, inner, 1), RGB(218, 211, 178));
    text(dc, L"玩家与角色", rect(lx, 453, inner, 32), 24, INK, true);

    RECT count = configPeopleRect();
    text(dc, L"玩家人数（2—4）", rect(lx, 531, 145, 25), 15, INK, true, DT_LEFT | DT_SINGLELINE);
    card(dc, count, RGB(253, 250, 235), RGB(207, 192, 145), 10);
    text(dc, std::to_wstring(s.playerCount), rect(count.left + 6, count.top + 4, 78, 44), 33, INK,
         true, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    button(dc, PeopleUp, L"▲", rect(count.right - 40, count.top, 40, 24), false,
           !s.teams && s.playerCount < 4, 13);
    button(dc, PeopleDown, L"▼", rect(count.right - 40, count.top + 28, 40, 24), false,
           !s.teams && s.playerCount > 2, 13);
    text(dc, s.teams ? L"2对2已锁定4人" : L"支持上下箭头与滚轮",
         rect(count.left - 12, count.bottom + 5, 162, 20), 12, MUTED, false,
         DT_CENTER | DT_SINGLELINE);
    text(dc, L"每人角色数", rect(margin + 335, 531, 115, 25), 17, INK, true);
    int roleX = margin + 460, roleWidth = (leftWidth - 498) / 2;
    button(dc, Single, L"1名", rect(roleX, 520, roleWidth, 44), teamCount() == 1, true, 18);
    button(dc, Duo, L"2名", rect(roleX + roleWidth + 12, 520, roleWidth, 44), teamCount() == 2,
           heroes, 18);

    text(dc, L"对战关系", rect(lx, 621, 138, 26), 18, INK, true);
    int relationWidth = (optionsWidth - 14) / 2;
    button(dc, FreeForAll, L"各自为战", rect(optionsX, 610, relationWidth, 46), !s.teams, true, 18);
    button(dc, Teams, L"2对2 · 自动设为4人",
           rect(optionsX + relationWidth + 14, 610, relationWidth, 46), s.teams, true, 17);
    if (s.teams)
        text(dc, L"玩家1与3同队，2与4同队。", rect(optionsX, 660, optionsWidth, 19), 12, MUTED);

    if (teaching) {
        text(dc, L"电脑风格", rect(lx, 693, 138, 26), 18, INK, true);
        int styleWidth = (optionsWidth - 28) / 3;
        button(dc, BotBalanced, L"均衡", rect(optionsX, 684, styleWidth, 44),
               s.botStyle == BotStyle::Balanced, true, 17);
        button(dc, BotAggressive, L"主动进攻",
               rect(optionsX + styleWidth + 14, 684, styleWidth, 44),
               s.botStyle == BotStyle::Aggressive, true, 17);
        button(dc, BotCautious, L"隐蔽反击",
               rect(optionsX + 2 * (styleWidth + 14), 684, styleWidth, 44),
               s.botStyle == BotStyle::Cautious, true, 17);
    } else {
        text(dc,
             s.networkMode ? L"房主创建房间后，分享地址给朋友加入。"
                           : L"每位玩家独立操作；完成布阵后开始对局。",
             rect(optionsX, 689, optionsWidth, 38), 16, MUTED);
    }
    fill(dc, rect(lx, 741, inner, 1), RGB(218, 211, 178));
    text(dc, L"辅助推理", rect(lx, 751, 128, 25), 17, INK, true);
    RECT toggle = rect(optionsX, 751, 90, 31);
    configurationAssistSwitch(dc, toggle);
    text(dc, s.assist ? L"显示保守的候选位置提示" : L"自行观察和判断对手位置",
         rect(optionsX + 108, 752, optionsWidth - 108, 22), 14, MUTED);

    const int practiceHeight = teaching ? 125 : 0,
              mapHeight = panelHeight - (teaching ? practiceHeight + 16 : 0);
    card(dc, rect(rightX, panelY, rightWidth, mapHeight), RGB(253, 250, 235), RGB(194, 184, 142),
         20);
    text(dc, L"地图选择", rect(rx, 213, rightInner, 31), 24, INK, true);
    int mapChoiceWidth = (rightInner - 14) / 2;
    button(dc, MapDefault, heroes ? L"默认5×5" : L"3×3 侦察地图", rect(rx, 254, mapChoiceWidth, 44),
           !s.customEnabled || !heroes, heroes, 17);
    button(dc, MapUse, L"自定义地图", rect(rx + mapChoiceWidth + 14, 254, mapChoiceWidth, 44),
           heroes && s.customEnabled, heroes, 17);
    int previewBottom = panelY + mapHeight - 81;
    configurationMapPreview(dc, rect(rx + 12, 313, rightInner - 24, previewBottom - 313), map);
    int editWidth = std::min(280, rightInner);
    button(dc, MapEdit, L"编辑 / 打开地图",
           rect(rightX + (rightWidth - editWidth) / 2, previewBottom + 9, editWidth, 34), false,
           heroes, 16);
    text(dc, heroes ? L"自定义地图：长宽各1—9格" : L"侦察模式固定单角色与3×3地图",
         rect(rx, previewBottom + 48, rightInner, 22), 13, MUTED, false, DT_CENTER | DT_SINGLELINE);
    if (teaching) {
        int practiceY = panelY + mapHeight + 16;
        card(dc, rect(rightX, practiceY, rightWidth, practiceHeight), RGB(253, 250, 235),
             RGB(194, 184, 142), 20);
        text(dc, L"练习入口", rect(rx, practiceY + 12, rightInner, 27), 21, INK, true);
        int practiceWidth = (rightInner - 14) / 2;
        button(dc, TutorialBasics, L"新手教程  ›", rect(rx, practiceY + 48, practiceWidth, 38),
               false, true, 18);
        button(dc, Skills, L"角色教程  ›",
               rect(rx + practiceWidth + 14, practiceY + 48, practiceWidth, 38), false, true, 18);
        configurationPracticeIcon(dc, rect(rx + 14, practiceY + 54, 26, 26), false);
        configurationPracticeIcon(dc, rect(rx + practiceWidth + 28, practiceY + 54, 26, 26), true);
        text(dc, L"熟悉操作", rect(rx, practiceY + 91, practiceWidth, 20), 13, MUTED, false,
             DT_CENTER | DT_SINGLELINE);
        text(dc, L"练习技能", rect(rx + practiceWidth + 14, practiceY + 91, practiceWidth, 20), 13,
             MUTED, false, DT_CENTER | DT_SINGLELINE);
    }

    card(dc, rect(margin, bottomY, total, 82), RGB(253, 249, 232), RGB(183, 163, 103), 18);
    text(dc, L"本局配置", rect(lx, bottomY + 25, 136, 31), 23, INK, true);
    fill(dc, rect(margin + 164, bottomY + 17, 1, 48), RGB(216, 204, 160));
    std::wstring summary = std::to_wstring(s.playerCount) + L"人 · 每人" +
                           std::to_wstring(teamCount()) + L"名角色 · " +
                           (s.teams ? L"2对2" : L"各自为战") + L" · " +
                           (s.customEnabled && heroes ? L"自定义" : L"默认") +
                           std::to_wstring(map.width) + L"×" + std::to_wstring(map.height);
    int beginWidth = std::min(480, total * 36 / 100),
        beginX = canvasWidth() - margin - beginWidth - 12;
    text(dc, summary, rect(margin + 188, bottomY + 29, beginX - margin - 208, 26), 17, INK, false,
         DT_SINGLELINE | DT_VCENTER);
    button(dc, Begin, s.networkMode ? L"创建 / 加入房间 →" : L"选择角色与站位 →",
           rect(beginX, bottomY + 12, beginWidth, 58), true, true, 26);
    if (!s.notice.empty())
        text(dc, s.notice, rect(margin + 8, bottomY - 16, total - 16, 18), 12, CORAL, true);
    footer(dc, L"选择人数、角色和地图后开始对局。");
}

void skillsPage(HDC dc) { // 角色特色和判定细节分区展示。
    card(dc, rect(36, 122, 284, 894));
    card(dc, rect(344, 122, 740, 894));
    for (int i = 0; i < 13; ++i) {
        button(dc, RoleBase + i, L"        " + roleName(Role(i)), rect(60, 145 + i * 58, 236, 49),
               s.selectedRole == Role(i), true, 22);
        characterAvatar(dc, rect(68, 152 + i * 58, 35, 35), Role(i), 1, true,
                        s.selectedRole == Role(i));
    } // 左栏选职业，右栏留足阅读空间。
    if (s.selectedRole == Role::HiddenWolf) {
        characterIllustration(dc, rect(370, 145, 108, 207), s.selectedRole, 0);
        characterIllustration(dc, rect(486, 145, 108, 207), s.selectedRole, 1);
        text(dc, L"人形", rect(370, 355, 108, 22), 14, MUTED, false, DT_CENTER);
        text(dc, L"狼形", rect(486, 355, 108, 22), 14, MUTED, false, DT_CENTER);
    } else {
        characterIllustration(dc, rect(376, 145, 210, 232), s.selectedRole);
    }
    text(dc, roleName(s.selectedRole), rect(614, 153, 424, 45), 28, TEAL, true);
    roleText(dc, roleStats(s.selectedRole), rect(614, 221, 424, 145), 19);
    fill(dc, rect(382, 394, 650, 1), LINE);
    text(dc, L"角色技能", rect(382, 407, 650, 30), 23, TEAL, true);
    roleText(dc, portraitSkillText(s.selectedRole), rect(382, 450, 650, 310), 18);
    card(dc, rect(368, 775, 691, 223), PALE);
    text(dc, L"tips", rect(382, 789, 650, 30), 23, TEAL, true);
    roleText(dc, roleTips(s.selectedRole), rect(382, 831, 650, 151), 17);
    button(dc, TutorialRole, L"开始该角色实操教程", rect(382, 1007, 650, 35), true, true, 18);
    footer(dc,
           L"V" + std::wstring(VersionText) +
               L" · 每回合选择一名角色，逐步行动；伤害上限在受击方回合开始重置。"); // 版本提示。
} // 角色说明页结束。

void room(HDC dc) { // 双机联机入口沿用地址直连。
    card(dc, rect(36, 122, 514, 599));
    card(dc, rect(574, 122, 510, 599));
    bool idle = !s.net || s.net->phase() == forest::Phase::Idle ||
                s.net->phase() == forest::Phase::Error; // 当前可否建立新连接。
    text(dc, L"创建房间", rect(64, 151, 450, 44), 27, INK, true);
    text(dc, L"加入朋友", rect(602, 151, 450, 44), 27, INK, true); // 两种网络身份。
    text(dc, L"房主决定地图、人数和阵营。所有人连通并完成秘密布阵后开始。", rect(64, 220, 450, 65),
         17, MUTED);
    text(dc, L"输入房主可达的 IPv4 地址，端口填写在左侧。", rect(602, 220, 450, 65), 17,
         MUTED); // 显示操作顺序。
    text(dc, L"端口（创建 / 加入共用）", rect(64, 285, 451, 27), 15, TEAL, true);
    text(dc, L"房主 IPv4 地址", rect(602, 285, 450, 27), 15, TEAL, true); // 输入框标签。
    card(dc, rect(65, 320, 210, 44));
    card(dc, rect(602, 320, 450, 44)); // 原生控件覆盖在这些框上。
    button(dc, NetHost, L"创建房间 · 等待朋友", rect(64, 395, 452, 52), true, idle);
    button(dc, NetJoin, L"连接房主", rect(602, 395, 450, 52), true,
           idle); // 连接请求使用非阻塞轮询。
    text(dc, L"可分享的本机地址", rect(64, 480, 450, 28), 16, INK, true);
    text(dc, s.addresses.empty() ? L"请查看你的虚拟局域网或网卡地址。" : s.addresses,
         rect(64, 519, 450, 87), 16, TEAL); // 地址仅供玩家选择和分享。
    text(dc,
         modeName(s.mode) + (heroMode(s.mode) ? L"；职业对战无侦察。" : L"；侦察不限次数。") +
             L"\n房主地图自动同步给加入者。",
         rect(64, 633, 450, 58), 15, MUTED); // 统一房间规则。
    text(dc,
         !s.notice.empty() ? s.notice
         : s.net           ? s.net->status()
                           : L"默认端口39091；同机测试填127.0.0.1。",
         rect(602, 485, 450, 118), 16, TEAL);
    button(dc, NetCancel, idle ? L"返回设置" : L"取消连接",
           rect(602, 633, 450, 48)); // 提供可恢复错误提示。
    button(dc, NetResume, L"恢复中断的对局…", rect(64, 760, 988, 48), false, true, 22);
    footer(
        dc,
        L"所有玩家需使用V" + std::wstring(VersionText) +
            L"。异地需可互通的虚拟局域网或公网映射；各客户端保存可信裁判备份。"); // 明确版本与连通前提。
} // 结束联机入口。
