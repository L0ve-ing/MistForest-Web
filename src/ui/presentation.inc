bool presentationBusy() {
    return s.playback.busy();
} // 最后一条的阅读和特效时间也属于播放阶段。

bool battleInputReady() {
    return (!s.tutorial || tutorialInputAllowed()) && s.page == Page::Battle &&
           s.phase == Phase::Playing && !presentationBusy() && s.inspected < 0 && !s.pendingLeave &&
           !s.playbackOptions;
}

int nextSelectableUnit(const View &v, int direction = 1) {
    if (v.teamSize < 2 || v.budget.active >= 0)
        return -1;
    int slot = std::clamp(s.choice.state().unit - v.player * 2, 0, v.teamSize - 1);
    for (int offset = 1; offset < v.teamSize; ++offset) {
        int id =
            v.player * 2 + (slot + offset * (direction > 0 ? 1 : -1) + v.teamSize) % v.teamSize;
        if (v.units[id].health > 0)
            return id;
    }
    return -1;
}

bool selectBattleUnit(int id) {
    if (!battleInputReady())
        return false;
    if (!s.choice.selectUnit(liveView(), id))
        return false;
    resetChoice();
    return true;
} // 轮换和鼠标共用真实预算；换人时清除尚未提交的旧路径。

void clearPresentation() {
    s.playback.reset();
    s.actorSprites = {};
    s.historyOffset = 0;
    s.historyTurn = -1;
    s.historyMeasuredTurn = -2;
    s.historyMeasuredMax = 0;
    s.inspected = -1;
} // 开局、视角交接和离开时不残留私人提示。

// All sources here are committed and viewer-filtered. Batch previews stay in
// currentView() and are never used to advance the playback revision.
View displayedView() {
    if (const auto *frame = s.playback.displayedFrame())
        return *frame;
    return currentView();
}

ULONGLONG playbackClock(ULONGLONG now) {
    return s.pendingLeave      ? s.leaveAt
           : s.inspected >= 0  ? s.inspectAt
           : s.playbackOptions ? s.optionsAt
                               : now;
}

void syncPresentation(const View &committed, ULONGLONG now = GetTickCount64()) {
    const auto &p = s.playback.state();
    bool fresh = p.viewer != committed.player || committed.revision < p.revision;
    if (fresh)
        clearPresentation();
    if (committed.revision == p.revision)
        return;
    std::vector<View> history;
    for (int revision = std::max(0, p.revision); revision <= committed.revision; ++revision) {
        if (std::any_of(p.frames.begin(), p.frames.end(),
                        [&](const View &frame) { return frame.revision == revision; }))
            continue;
        if (s.online && s.net) {
            const auto &frames = s.net->history();
            if (std::size_t(revision) < frames.size() &&
                frames[revision].player == committed.player)
                history.push_back(frames[revision]);
        } else if (s.game && std::size_t(revision) < s.game->snapshots().size() &&
                   s.game->state().revision >= committed.revision)
            history.push_back(s.game->historicalView(committed.player, revision));
    }
    s.playback.setSpeed(s.playbackSpeed);
    s.playback.sync(committed, history, playbackClock(now));
}

bool advancePresentation(ULONGLONG now) {
    if (s.pendingLeave || s.inspected >= 0 || s.playbackOptions)
        return false;
    return s.playback.advance(now);
}

double playbackProgress(ULONGLONG now = GetTickCount64()) {
    return s.playback.progress(playbackClock(now));
}

int noticeHeight(HDC dc, const std::wstring &value, int fontSize, int width = 324) {
    HFONT font = CreateFontW(-fontSize, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Microsoft YaHei UI");
    auto old = SelectObject(dc, font);
    RECT r = rect(0, 0, width, 0);
    DrawTextW(dc, value.c_str(), int(value.size()), &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    DeleteObject(font);
    return std::max(fontSize, int(r.bottom - r.top)) + 4;
} // 按真实换行测量每条文字，防止长判定被截断。

#include "battle_log.inc"
