void leaveModal(HDC dc);

// Cache only menu pages: battle/replay clocks and committed state always keep rendering.
// The key includes visible settings and interaction state, so input never reuses stale hits.
#ifdef _WIN32
auto menuFrameKey() {
    return std::make_tuple(s.page, canvasWidth(), canvasHeight(), s.scale, s.offsetX, s.offsetY,
                          visual.hover, visual.pressed,
                          s.mode, s.playerCount, s.teamSize, s.teams, s.bot, s.networkMode,
                          s.botStyle, s.series, s.assist, s.customEnabled, s.editor.document.seed(),
                          s.customMap.width, s.customMap.height, s.customMap.cells,
                          s.customMap.horizontal, s.customMap.vertical, s.notice,
                          bool(s.last), s.online,
                          s.net ? int(s.net->phase()) : -1);
}

struct MenuFrame {
    decltype(menuFrameKey()) key{};
    std::vector<Hit> hits;
    bool valid = false;
};

MenuFrame &menuFrame() {
    static MenuFrame frame;
    return frame;
}

bool restoreMenuFrame(HDC dc) {
    if (s.page != Page::Lobby && s.page != Page::Config)
        return false;
    auto &cache = menuFrame();
    auto &frame = pageFrames().latest;
    auto now = visualNow();
    if (introOpacity(now) <= 0 || !cache.valid || cache.key != menuFrameKey() ||
        !frame.dc || frame.page != s.page ||
        frame.width != canvasWidth() || frame.height != canvasHeight())
        return false;
    int saved = SaveDC(dc);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);
    StretchBlt(dc, 0, 0, frame.width, frame.height, frame.dc, 0, 0,
               frame.width, frame.height, SRCCOPY);
    RestoreDC(dc, saved);
    s.hits = cache.hits;
    return true;
}

void rememberMenuFrame() {
    auto &cache = menuFrame();
    cache.valid = (s.page == Page::Lobby || s.page == Page::Config) &&
                  pageFrames().latest.dc && pageFrames().latest.page == s.page;
    if (cache.valid) {
        cache.key = menuFrameKey();
        cache.hits = s.hits;
    }
}
#endif

void draw(HDC dc) {
    s.actorSprites.clock(
        visualNow(), ((s.page == Page::Battle && s.phase != Phase::Cover && s.historyTurn < 0) ||
                      s.page == Page::Replay) &&
                         s.inspected < 0 && !s.pendingLeave && !s.playbackOptions);
#ifdef _WIN32
    bool cached = restoreMenuFrame(dc);
#else
    bool cached = false;
#endif
    if (!cached) {
        s.hits.clear();
        fill(dc, rect(0, 0, canvasWidth(), canvasHeight()), BG);
        forestBackground(dc, visualNow());
        header(dc);
        switch (s.page) {
        case Page::Tutorial:
            skillsPage(dc);
            break;
        case Page::Lobby:
            lobby(dc);
            break;
        case Page::Config:
            configuration(dc);
            break;
        case Page::Battle:
            battle(dc);
            portraitTip(dc);
            roleDetails(dc);
            break;
        case Page::Network:
            room(dc);
            break;
        case Page::Editor:
            editor(dc);
            break;
        case Page::Skills:
            skillsPage(dc);
            break;
        case Page::Replay:
            replayPage(dc);
            break;
        }
        if (s.page == Page::Battle && s.inspected < 0 && s.playback.state().hurtLive &&
            s.historyTurn < 0)
            damageEdges(dc, effectOpacity(s.playback.state().hurtAt, feedbackNow(), 420));
        rememberPageFrame(dc);
#ifdef _WIN32
        rememberMenuFrame();
#endif
    }
    pageCrossfade(dc, visualNow());
    introFog(dc, visualNow());
    if (s.moreActions && s.page == Page::Battle) {
        s.hits.clear();
        card(dc, rect(1247, 742, 212, 113), WHITE, GOLD, 12);
        text(dc, L"更多操作", rect(1261, 752, 184, 22), 15, TEAL, true);
        button(dc, Surrender, L"投降…", rect(1261, 781, 184, 30), false, battleInputReady());
        button(dc, MoreActions, L"关闭 [Esc]", rect(1261, 816, 184, 27));
    }
    leaveModal(dc);
    playbackOptionsModal(dc);
} // 绘图重新建立当帧可用按钮。

void leaveModal(HDC dc) {
    if (!s.pendingLeave)
        return;
#ifdef _WIN32
    {
        art::Canvas canvas(dc);
        Gdiplus::SolidBrush dim(Gdiplus::Color(150, 13, 31, 24));
        canvas.graphics->FillRectangle(&dim, 0, 0, canvasWidth(), canvasHeight());
    }
#endif
    s.hits.clear();
    int x = (canvasWidth() - 600) / 2, y = (canvasHeight() - 350) / 2;
    card(dc, rect(x, y, 600, 350), RGB(249, 248, 235), GOLD, 24, 2);
    forestCrest(dc, rect(x + 264, y + 23, 72, 72));
    text(dc, L"离开当前对局？", rect(x + 40, y + 114, 520, 44), 29, INK, true,
         DT_CENTER | DT_SINGLELINE);
    text(dc, L"离开后将结束当前对局。\n你也可以留在这里，继续查看战场与行动。",
         rect(x + 45, y + 177, 510, 62), 19, MUTED, false, DT_CENTER | DT_WORDBREAK);
    button(dc, LeaveCancel, L"留在对局 [Esc]", rect(x + 38, y + 273, 253, 48), true, true, 19);
    button(dc, LeaveConfirm, L"确认离开 [Enter]", rect(x + 309, y + 273, 253, 48), false, true, 19);
}
