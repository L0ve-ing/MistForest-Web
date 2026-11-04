View liveView() {
    return s.online ? s.net->view() : s.game->view(s.viewer);
} // 战报和实际提交只读取已确认状态。

View currentView() {
    View v = liveView();
    if (s.choice.state().batching && !s.choice.state().planned.empty()) {
        if (s.online) {
            s.net->previewBatch(s.choice.state().planned, v);
        } else {
            s.game->previewBatch(s.choice.state().planned, s.viewer, v);
        }
    }
    return v;
}

Map selectedMap() {
    return heroMode(s.mode) && s.customEnabled ? s.customMap : defaultMap(s.mode);
}

Map battleMap() {
    return s.online && s.net ? s.net->terrain() : s.roundMap;
} // 所有进行中信息来自受限视图。

int teamCount() {
    return heroMode(s.mode) ? s.teamSize : 1;
} // 三格固定单角色，五格按大厅选择。

std::wstring playerName(int p) {
    return L"玩家" + std::to_wstring(p + 1);
}

std::wstring modeName(Mode m) {
    return std::to_wstring(size(m)) + L"×" + std::to_wstring(rows(m)) +
           (heroMode(m) ? L" 职业" : L" 侦察") + L" · " + std::to_wstring(s.playerCount) + L"人" +
           (s.teams ? L" · 2对2" : L" · 混战");
} // 明确区分玩家数量与每人角色数。

void resetChoice() {
    s.actionScroll = 0;
    s.previousLog = false;
    s.notice.clear();
    if ((s.online && s.net && s.net->hasView()) || (!s.online && s.game))
        s.choice.reset(currentView());
    else
        s.choice = mist::selection::ActionSelection{};
} // 当前一步完成后重新选择，不保留旧目标防止连点误执行。
