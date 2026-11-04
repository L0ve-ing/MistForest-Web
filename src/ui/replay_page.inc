void replayPage(HDC dc) {
    View v;
    bool full = !s.replayPrivate && s.last;
    if (full) {
        const auto &snap = s.last->snapshots().at(s.replay);
        v = s.last->historicalView(s.viewer, s.replay);
        v.units = snap.units;
    } else if (!s.replayViews.empty())
        v = s.replayViews.at(s.replay);
    else
        return;
    card(dc, rect(36, 122, 570, 599));
    card(dc, rect(638, 122, 446, 599));
    text(dc, full ? L"完整对局复盘" : L"中断记录 · 本人视角", rect(64, 148, 510, 43), 26, INK,
         true);
    board(dc, v.mode, v.map, v.units, full);
    text(dc, L"步骤" + std::to_wstring(s.replay), rect(664, 150, 393, 42), 27, TEAL, true);
    std::wstring report = v.turnReport;
    if (full && s.replay > 0) {
        const auto &e = s.last->events()[s.replay - 1];
        report = e.publicText + L"\n" + e.privateText + L"\n" + e.feedback;
        for (int p = 0; p < v.playerCount; ++p)
            if (!e.playerNotes[p].empty())
                report += L"\n" + playerName(p) + L"：" + e.playerNotes[p];
    }
    if (s.replayActors.busy()) {
        report.clear();
        const auto &p = s.replayActors.state();
        for (std::size_t i = p.actionBegin; i < p.shown && i < p.lines.size(); ++i)
            report += p.lines[i].text + L"\n";
    }
    text(dc, report, rect(664, 215, 393, 340), 18, INK);
    button(dc, ReplayPrev, L"上一步", rect(664, 609, 190, 48), false, s.replay > 0);
    button(dc, ReplayNext, L"下一步", rect(867, 609, 190, 48), true,
           s.replay < int(full ? s.last->events().size() : s.replayViews.size() - 1));
    button(dc, Export, L"导出对局记录", rect(664, 670, 393, 38));
    if (s.online)
        button(dc, ReturnBattle, L"返回恢复中的对局", rect(664, 733, 393, 42));
    footer(dc,
           full ? L"对局已结束或正式终止，全部位置公开。" : L"连接仍可恢复，敌方位置保持隐藏。");
}
