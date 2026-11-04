std::filesystem::path recoveryFile() { // 会话恢复文件独立于复盘文本。
    wchar_t exe[32768]{};
    GetModuleFileNameW(nullptr, exe, 32768);
    return std::filesystem::path(exe).parent_path() / L"mist-session.recovery";
}

void openReplay() { // 恢复失败后开放全知记录，恢复期间只使用本人历史。
    s.replayViews.clear();
    s.replayPrivate = false;
    s.replay = 0;
    s.replayFxStep = -1;
    s.replayAttacks = {};
    s.replayEffect = {};
    s.replayEffects.clear();
    if (s.online && s.net) {
        if (auto full = s.net->replay()) {
            s.last = full;
            s.lastOnline = true;
        } else if (s.net->phase() == forest::Phase::Recovering) {
            s.replayViews = s.net->history();
            s.replayPrivate = true;
        } else if (s.net->phase() != forest::Phase::Finished &&
                   s.net->phase() != forest::Phase::Aborted)
            return;
    }
    if ((s.replayPrivate && s.replayViews.empty()) || (!s.replayPrivate && !s.last))
        return;
    changePage(Page::Replay);
    selectReplayStep(0);
    invalidate();
}

void submitPlan() { // 在真实状态上原子校验，失败不执行部分路径。
    if (!battleInputReady() || s.choice.state().planned.empty())
        return;
    auto actions = s.choice.state().planned;
    std::size_t eventStart = s.online ? 0 : s.game->events().size();
    bool ok = s.online ? s.net->actBatch(actions) : s.game->actBatch(actions, &s.notice);
    if (!ok)
        return;
    s.choice.clearPlan();
    if (s.online)
        syncNetwork();
    else {
        for (std::size_t i = eventStart; i < s.game->events().size(); ++i)
            tutorialProgress(s.game->events()[i].action);
        afterLocal();
    }
}

#include "tutorial.inc" // 分步教学与真实裁判结果。

void resumeNetwork() { // 可直接加载程序旁最近保存的会话。
    if (!s.net)
        s.net = std::make_unique<forest::Session>();
    if (!s.net->loadRecovery(recoveryFile())) {
        s.notice = L"恢复失败：未找到有效会话，或恢复端口被占用。";
        invalidate();
        return;
    }
    s.online = true;
    s.tutorial = false;
    s.savedRevision = -1;
    s.choice.clearPlan();
    SetTimer(s.window, 2, 30, nullptr);
    syncNetwork();
}

void exportAnyReplay() { // 仍在恢复期间的导出只包含本人可见记录。
    if (!s.replayPrivate) {
        exportReplay();
        return;
    }
    try {
        auto folder = recoveryFile().parent_path() / L"replays";
        std::filesystem::create_directories(folder);
        std::ofstream out(folder / (L"mist-private-" + std::to_wstring(GetTickCount64()) + L".txt"),
                          std::ios::binary);
        out << "\xEF\xBB\xBF" << utf8(L"中断记录·本人视角\n");
        for (const auto &v : s.replayViews) {
            out << utf8(L"\n步骤" + std::to_wstring(v.revision) + L"\n" + v.turnReport + L"\n");
            for (int k = 0; k < v.teamSize; ++k) {
                const auto &u = v.units[v.player * 2 + k];
                out << utf8(stats(u) + L" " + coordinate(v.mode, u.position) + L"\n");
            }
        }
        out.flush();
        s.notice = out ? L"已导出本人视角记录。" : L"导出失败。";
    } catch (const std::exception &) {
        s.notice = L"导出失败，请检查目录权限。";
    }
}

void selectReplayStep(int step) {
    int limit = s.replayPrivate ? int(s.replayViews.size()) - 1
                : s.last        ? int(s.last->events().size())
                                : 0;
    step = std::clamp(step, 0, std::max(0, limit));
    if (step == s.replay && s.replayFxStep == step)
        return; // Repaint / a clamped key never restarts the effect.
    s.replay = step;
    s.replayFxStep = step;
    s.replayAttacks = {};
    s.replayEffect = {};
    s.replayEffects.clear();
    s.replayFxAt = visualNow();
    s.replayMovements.clear();
    s.replayActors.reset();
    s.replayActors.setSpeed(s.playbackSpeed);
    s.replayRate = s.playbackSpeed / 100.;
    if (!step)
        return;
    if (!s.replayPrivate && s.last)
        s.replayAttacks = s.last->events()[step - 1].attackCells;
    else if (s.replayPrivate && step < int(s.replayViews.size())) {
        const auto &v = s.replayViews[step];
        for (const auto &n : v.notices)
            if (n.revision == v.revision)
                for (int c = 0; c < 81; ++c)
                    s.replayAttacks[c] = s.replayAttacks[c] || n.attacks[c];
    }
    View before =
        s.replayPrivate ? s.replayViews[step - 1] : s.last->historicalView(s.viewer, step - 1);
    View after = s.replayPrivate ? s.replayViews[step] : s.last->historicalView(s.viewer, step);
    const Notice *header = nullptr;
    for (const auto &notice : after.notices)
        if (notice.revision == after.revision && (!header || !notice.feedback)) {
            header = &notice;
            if (!notice.feedback)
                break;
        }
    if (header) {
        View motionBefore = before, motionAfter = after;
        if (!s.replayPrivate) {
            // Full replay has already disclosed the referee. Recovery replay
            // takes the filtered branch above and never touches these snapshots.
            motionBefore.units = s.last->snapshots()[step - 1].units;
            motionAfter.units = s.last->snapshots()[step].units;
        }
        motionBefore.notices.clear();
        motionAfter.notices.erase(
            std::remove_if(motionAfter.notices.begin(), motionAfter.notices.end(),
                           [&](const Notice &n) { return n.revision != step; }),
            motionAfter.notices.end());
        s.replayActors.sync(motionBefore, {motionBefore}, visualNow());
        s.replayActors.sync(motionAfter, {motionBefore, motionAfter}, visualNow());
        s.replayMovements = s.replayActors.movements();
        s.replayFxAt = s.replayActors.state().nextAt;
        // Full replay is opened only after the referee has been revealed. The actor's
        // filtered history may then show their movement effect; private recovery
        // replay must keep using the observer's filtered history instead.
        View fxBefore = s.replayPrivate ? before : s.last->historicalView(header->player, step - 1);
        View fxAfter = s.replayPrivate ? after : s.last->historicalView(header->player, step);
        // Use the same phase boundaries as text and actor feedback. Independent
        // wall-clock phases would run ahead after a delayed frame or a modal.
        for (const auto &phase : s.replayActors.state().phases)
            s.replayEffects.push_back(
                makeSkillFx(fxBefore, fxAfter, *header, phase.attacks, fxAfter.notices));
        if (!s.replayEffects.empty())
            s.replayEffect = s.replayEffects.front();
    }
}
