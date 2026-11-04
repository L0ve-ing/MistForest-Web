// 战报与局内资料均读取受限视图。
COLORREF noticeColor(const Notice &line) {
    return line.feedback ? CORAL : TEAL;
}

bool noticeGroupStart(const std::vector<std::size_t> &visible, std::size_t index) {
    if (!index)
        return true;
    const auto &a = s.playback.state().lines[visible[index - 1]];
    const auto &b = s.playback.state().lines[visible[index]];
    return a.player != b.player || a.unit != b.unit;
}

std::wstring noticeGroup(const Notice &line, const View &v) {
    return playerName(line.player) + L" · " +
           (line.unit >= 0 && line.unit < int(v.units.size())
                ? roleName(v.units[line.unit].role) + L" " + std::to_wstring(line.unit % 2 + 1)
                : L"回合行动");
}

void portrait(HDC dc, int unit, RECT r, const View &v) {
    const auto &u = v.units[unit];
    characterAvatar(dc, r, u.role, u.form, unit / 2 == s.viewer, false);
    shadowRing(dc, r, shadowMark(v, unit, -1), true);
    s.hits.push_back({r, DetailsBase + unit, true});
}

void feedPanel(HDC dc) {
    card(dc, rect(24, 94, 402, 801), RGB(253, 250, 238));
    text(dc, L"战场信息", rect(46, 111, 240, 38), 27, INK, true);
    text(dc, L"点击头像查看技能", rect(272, 123, 132, 23), 13, MUTED);
    int rosterRows = 0;
    View v;
    bool has = (s.online && s.net && s.net->hasView()) || (!s.online && s.game);
    if (has) {
        v = displayedView();
        int row = 0;
        for (int player = 0; player < v.playerCount; ++player)
            if (player != s.viewer)
                for (int k = 0; k < v.teamSize; ++k) {
                    int id = player * 2 + k;
                    const auto &u = v.units[id];
                    int y = 159 + row++ * 30;
                    rosterRows = row;
                    portrait(dc, id, rect(46, y, 29, 24), v);
                    text(dc,
                         playerName(player) +
                             (allied(v.teams, player, s.viewer) ? L"·友 " : L"·敌 ") +
                             roleName(u.role) +
                             (u.role == Role::HiddenWolf ? (u.form ? L"·狼" : L"·人") : L"") +
                             L" " + (u.health > 0 ? L"生命" + halfPoints(u.health) : L"阵亡") +
                             (u.armor ? L" 甲" + halfPoints(u.armor * 2) : L"") +
                             (u.shield ? L" 临甲" + halfPoints(u.shield) : L""),
                         rect(82, y + 1, 320, 24), 14, u.health > 0 ? INK : MUTED, true);
                }
    }
    const int navY = 175 + rosterRows * 30;
    const int logTop = navY + 78;
    auto &p = s.playback.state();
    int latest = p.shown ? p.lines[p.shown - 1].turn : -1,
        wanted = s.historyTurn < 0 ? latest : s.historyTurn;
    bool previous = false, next = false;
    for (std::size_t i = 0; i < p.shown; ++i) {
        previous |= p.lines[i].turn < wanted;
        next |= p.lines[i].turn > wanted;
    }
    button(dc, LogPrevious, L"上一回合", rect(46, navY, 123, 32), false, previous, 16);
    button(dc, LogNext, L"下一回合", rect(177, navY, 123, 32), false, next, 16);
    button(dc, LogCurrent, L"最新", rect(308, navY, 94, 32), s.historyTurn < 0, true, 16);
    text(dc,
         wanted >= 0
             ? (s.historyTurn < 0 ? L"第" : L"查看第") + std::to_wstring(wanted + 1) + L"回合"
             : L"对局记录",
         rect(48, navY + 43, 176, 25), 16, TEAL, true);
    text(dc, L"行动", rect(278, navY + 44, 50, 22), 14, TEAL, true);
    text(dc, L"反馈", rect(346, navY + 44, 50, 22), 14, CORAL, true);
    std::vector<std::size_t> visible;
    for (std::size_t i = 0; i < p.shown; ++i)
        if (p.lines[i].turn == wanted)
            visible.push_back(i);

    struct ActionCard {
        std::size_t first, last;
        int height;
        bool group, current;
    };

    std::vector<ActionCard> cards;
    int total = 0;
    for (std::size_t k = 0; k < visible.size();) {
        std::size_t end = k + 1;
        while (end < visible.size() &&
               p.lines[visible[end]].revision == p.lines[visible[k]].revision)
            ++end;
        bool current = p.running && visible[k] == p.actionBegin;
        int height = 30;
        for (auto j = k; j < end; ++j)
            height += noticeHeight(dc, p.lines[visible[j]].text, 17) + 3;
        if (current && p.shown < p.actionEnd)
            height += 26; // 给尚未显示的结果留出位置，卡片不会频繁跳动。
        height = std::max(current ? 89 : 79, height + 14);
        bool group = noticeGroupStart(visible, k);
        cards.push_back({k, end, height, group, current});
        total += height + 9 + (group ? 34 : 0);
        k = end;
    }
    int maxScroll = std::max(0, total - (800 - logTop));
    if (s.historyTurn >= 0 && s.historyMeasuredTurn == wanted)
        s.historyOffset += maxScroll - s.historyMeasuredMax;
    s.historyMeasuredTurn = wanted;
    s.historyMeasuredMax = maxScroll;
    s.historyOffset = std::clamp(s.historyOffset, 0, maxScroll);
    int y = logTop - std::max(0, maxScroll - s.historyOffset), saved = SaveDC(dc);
    IntersectClipRect(dc, 40, logTop, 411, 800);
    int sequence = 0;
    for (const auto &item : cards) {
        const auto &first = p.lines[visible[item.first]];
        if (item.group) {
            if (first.unit >= 0 && first.unit < int(v.units.size()))
                characterAvatar(dc, rect(48, y + 1, 28, 28), v.units[first.unit].role,
                                v.units[first.unit].form, first.player == s.viewer, false);
            text(dc, noticeGroup(first, v) + L" · 行动记录", rect(82, y + 3, 318, 25), 15, INK,
                 true);
            y += 34;
            sequence = 0;
        }
        card(dc, rect(44, y, 362, item.height),
             item.current ? RGB(238, 249, 242) : RGB(247, 249, 246), item.current ? TEAL : LINE, 12,
             item.current ? 2 : 1);
        text(dc, L"行动 " + std::to_wstring(++sequence), rect(57, y + 9, 174, 22), 13,
             item.current ? TEAL : MUTED, true);
        if (item.current) {
            card(dc, rect(311, y + 8, 81, 22), TEAL, TEAL, 6);
            text(dc, L"当前行动", rect(312, y + 9, 79, 20), 12, WHITE, true,
                 DT_CENTER | DT_SINGLELINE);
        }
        int lineY = y + 28;
        for (auto k = item.first; k < item.last; ++k) {
            const auto &n = p.lines[visible[k]];
            int h = noticeHeight(dc, n.text, 17);
            card(dc, rect(53, lineY - 1, 339, h + 2),
                 noticeColor(n) == CORAL ? RGB(250, 235, 227) : RGB(228, 240, 224),
                 noticeColor(n) == CORAL ? RGB(250, 235, 227) : RGB(228, 240, 224), 6);
            text(dc, n.text, rect(57, lineY, 324, h), 17, noticeColor(n), n.weight > 0);
            lineY += h + 3;
        }
        if (item.current && p.shown < p.actionEnd)
            text(dc, L"正在结算…", rect(57, lineY, 324, 23), 14, MUTED);
        if (item.current) {
            fill(dc, rect(57, y + item.height - 9, 336, 3), LINE);
            int width = int(336 * playbackProgress());
            if (width)
                fill(dc, rect(57, y + item.height - 9, width, 3), TEAL);
        }
        y += item.height + 9;
    }
    if (visible.empty())
        text(dc, L"等待行动。", rect(48, logTop + 21, 345, 80), 22, MUTED);
    RestoreDC(dc, saved);
    text(dc, L"滚轮浏览 · A 上一回合 · D 下一回合", rect(46, 814, 350, 28), 14, MUTED);
    card(dc, rect(46, 849, 356, 32), PALE);
    text(dc,
         s.inspected >= 0     ? L"查看技能中 · 播放暂停"
         : presentationBusy() ? L"按行动播放中 · 完成后开放行动"
                              : L"当前回合信息已显示完毕",
         rect(54, 854, 340, 24), 14, TEAL, true, DT_CENTER | DT_SINGLELINE);
}

void closeDetails(ULONGLONG now = GetTickCount64()) {
    if (s.inspected < 0)
        return;
    s.playback.shiftTime(now - s.inspectAt);
    s.inspected = -1;
    invalidate();
}

void roleDetails(HDC dc) {
    if (s.inspected < 0)
        return;
    auto v = currentView();
    if (s.inspected >= v.playerCount * 2)
        return;
    auto role = v.units[s.inspected].role;
    s.hits.clear(); // 说明浮层不允许点击穿透到棋盘或战斗按钮。
    card(dc, rect(392, 120, 726, 772), WHITE, TEAL, 22, 2);
    button(dc, CloseDetails, L"关闭 [Esc]", rect(968, 143, 126, 35), false, true, 15);
    characterAvatar(dc, rect(424, 144, 48, 48), role, v.units[s.inspected].form, true, true);
    text(dc,
         playerName(s.inspected / 2) + L" · " + roleName(role) + L" " +
             std::to_wstring(s.inspected % 2 + 1),
         rect(487, 148, 464, 42), 25, TEAL, true);

    characterIllustration(dc, rect(420, 207, 210, 282), role, v.units[s.inspected].form);
    roleText(dc, roleStats(role), rect(424, 499, 206, 107), 16);
    roleText(dc, portraitSkillText(role), rect(654, 211, 422, 395), 17);
    card(dc, rect(414, 621, 682, 199), PALE);
    text(dc, L"tips", rect(432, 635, 646, 26), 18, TEAL, true);
    roleText(dc, roleTips(role), rect(432, 672, 646, 133), 16);
    text(dc, L"查看期间暂停行动播放，关闭后接着显示。", rect(424, 846, 652, 25), 15, MUTED);
}
