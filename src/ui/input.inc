void keyboard(WPARAM key) {
    if (s.moreActions) {
        if (key == VK_ESCAPE)
            command(MoreActions);
        return;
    }
    if (s.playbackOptions) {
        if (key == VK_ESCAPE || key == VK_RETURN)
            command(ClosePlaybackOptions);
        return;
    }
    if (s.pendingLeave) {
        if (key == VK_ESCAPE)
            command(LeaveCancel);
        else if (key == VK_RETURN)
            command(LeaveConfirm);
        return;
    }
    if (s.inspected >= 0) {
        if (key == VK_ESCAPE || key == 'H')
            closeDetails();
        return;
    }
    int id = 0;
    if (key == VK_RETURN || key == VK_SPACE)
        id = s.phase == Phase::Cover ? Ready : s.phase == Phase::Hold ? Pass : Commit;
    if (s.page == Page::Battle) {
        for (const auto &hit : s.hits)
            if (shortcut(hit.id) == int(key) && hit.enabled) {
                id = hit.id;
                break;
            }
    }
    for (auto hit : s.hits)
        if (hit.id == id && hit.enabled) {
            command(id);
            break;
        }
} // 快捷键仅触发当前有效按钮。

void wheelAt(POINT point, int delta) {
    if (s.pendingLeave || s.playbackOptions || s.moreActions)
        return;
    if (s.page == Page::Config) {
        RECT selector = configPeopleRect();
        if (!s.teams && PtInRect(&selector, point)) {
            if (s.wheelZone != 5)
                s.wheelDelta = 0;
            s.wheelZone = 5;
            s.wheelDelta += delta;
            int steps = s.wheelDelta / 120;
            s.wheelDelta %= 120;
            s.playerCount = std::clamp(s.playerCount + steps, 2, 4);
            invalidate();
        } else
            s.wheelDelta = s.wheelZone = 0;
        return;
    }
    if (s.page != Page::Battle || !hasView() || s.inspected >= 0) {
        s.wheelDelta = s.wheelZone = 0;
        return;
    }
    int zone = 0;
    RECT feed = rect(24, 94, 402, 801), roles = rect(1066, 104, 393, 209), parameters{};
    bool amounts = false, payment = false;
    std::vector<int> choices;
    for (const auto &hit : s.hits) {
        if (hit.id >= Amount1 && hit.id <= Amount3) {
            if (!amounts)
                parameters = hit.rect;
            else {
                parameters.left = std::min(parameters.left, hit.rect.left);
                parameters.top = std::min(parameters.top, hit.rect.top);
                parameters.right = std::max(parameters.right, hit.rect.right);
                parameters.bottom = std::max(parameters.bottom, hit.rect.bottom);
            }
            amounts = true;
            if (hit.enabled)
                choices.push_back(hit.id);
        }
        if (hit.id == CostDown || hit.id == CostUp)
            payment = true;
    }
    RECT cost = rect(1066, 563, 393, 28);
    if (PtInRect(&feed, point))
        zone = 1;
    else if (PtInRect(&roles, point))
        zone = 2;
    else if (amounts && PtInRect(&parameters, point))
        zone = 3;
    else if (payment && PtInRect(&cost, point))
        zone = 4;
    if (!zone || (zone != 1 && !battleInputReady())) {
        s.wheelDelta = s.wheelZone = 0;
        return;
    }
    if (zone != s.wheelZone) {
        s.wheelDelta = 0;
        s.wheelZone = zone;
    }
    s.wheelDelta += delta;
    int steps = s.wheelDelta / 120;
    s.wheelDelta %= 120;
    if (!steps)
        return;
    if (zone == 1) {
        if (steps > 0 && s.historyTurn < 0 && s.playback.state().shown)
            s.historyTurn = s.playback.state().lines[s.playback.state().shown - 1].turn;
        s.historyOffset = std::max(0, s.historyOffset + steps * 72);
        invalidate();
        return;
    }
    if (zone == 2) {
        for (int i = 0; i < std::abs(steps); ++i)
            if (!selectBattleUnit(nextSelectableUnit(liveView(), steps > 0 ? -1 : 1)))
                break;
        invalidate();
        return;
    }
    if (zone == 4) {
        for (int i = 0; i < std::min(8, std::abs(steps)); ++i)
            command(steps > 0 ? CostUp : CostDown);
        return;
    }
    if (choices.empty())
        return;
    std::sort(choices.begin(), choices.end());
    auto v = currentView();
    int offset = s.choice.amountForSlot(v, 0);
    int index = 0;
    for (int i = 0; i < int(choices.size()); ++i)
        if (choices[i] - Amount1 + offset == s.choice.state().amount)
            index = i;
    command(choices[std::clamp(index + steps, 0, int(choices.size()) - 1)]);
} // 滚轮只选择角色或参数，棋盘不响应；消息坐标由窗口入口统一转换。
