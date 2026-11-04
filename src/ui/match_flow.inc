bool active() {
    return s.online ? s.net && s.net->phase() != forest::Phase::Idle &&
                          s.net->phase() != forest::Phase::Error &&
                          s.net->phase() != forest::Phase::Finished
                    : s.page == Page::Battle && s.phase != Phase::Finished;
} // 布阵也属于进行中的局。

bool navigate(Page page) {
    if (active() && s.window && !s.allowLeave) {
        s.pendingLeave = s.currentCommand;
        s.leaveAt = visualNow();
        invalidate();
        return false;
    }
    if (s.window)
        KillTimer(s.window, 1);
    if (s.net && (active() || page != Page::Replay))
        s.net->close();
    clearPresentation();
    changePage(page);
    s.notice.clear();
    s.choice.pointAt(0);
    return true;
} // 用户主动离开时保留未结束局的确认。

void prepareRound() {
    clearPresentation();
    s.online = false;
    s.tutorial = false;
    s.choice.clearPlan();
    s.game.reset();
    s.roundMap = selectedMap();
    if (heroMode(s.mode))
        s.mode = mapMode(s.roundMap);
    s.starts = {};
    s.formations = {};
    s.setupPiece = 0;
    s.setupPlayer = 0;
    s.setupSlot = 0;
    s.viewer = 0;
    s.choice = mist::selection::ActionSelection{};
    s.selectedRole = Role::Sniper;
    s.coverSetup = true;
    changePage(Page::Battle);
    s.phase = s.bot ? Phase::Setup : Phase::Cover;
    invalidate();
} // 历史程序不受本局状态影响。

void afterLocal() {
    s.choice.clearPlan();
    syncPresentation(s.game->view(s.viewer));
    resetChoice();
    if (s.game->finished()) {
        s.last = s.game;
        s.lastOnline = false;
        s.phase = Phase::Finished;
        if (s.game->state().winner < 4)
            ++s.score[s.game->state().winner];
    } else if (s.bot) {
        s.viewer = 0;
        if (inputPlayer(s.game->state()) != 0) {
            s.phase = Phase::Waiting;
            if (s.window)
                SetTimer(s.window, 1, 200, nullptr);
        } else
            s.phase = Phase::Playing;
    } else
        s.phase = inputPlayer(s.game->state()) == s.viewer ? Phase::Playing : Phase::Hold;
    invalidate();
} // 同回合继续行动不切换视角；结束后才交接。

void setupLocal() {
    int id = s.viewer * 2 + s.setupSlot;
    int start = s.selectedRole == Role::WallGuard ? s.formations[id][0] : s.choice.state().selected;
    if (!valid(s.mode, start, s.roundMap) ||
        (s.selectedRole == Role::WallGuard &&
         !validFormation(s.mode, s.roundMap, start, s.formations[id])))
        return;
    s.starts[id] = start;
    s.roles[id] = s.selectedRole;
    if (s.selectedRole != Role::WallGuard)
        s.formations[id] = {};
    s.choice.pointAt(0);
    s.setupPiece = 0;
    if (teamCount() == 2 && s.setupSlot == 0) {
        s.setupSlot = 1;
        s.selectedRole = s.roles[id] == Role::Sniper ? Role::Wolf : Role::Sniper;
        invalidate();
        return;
    }
    if (teamCount() == 1) {
        s.starts[s.viewer * 2 + 1] = s.starts[s.viewer * 2];
        s.roles[s.viewer * 2 + 1] = s.roles[s.viewer * 2];
    }
    s.setupSlot = 0;
    if (s.bot) {
        for (int i = 2; i < s.playerCount * 2; ++i) {
            do {
                s.starts[i] = 1 + int(s.rng() % (cellCount(s.mode)));
            } while (!valid(s.mode, s.starts[i], s.roundMap));
            do {
                s.roles[i] = Role(s.rng() % 13);
            } while (
                (i % 2 == 1 && s.roles[i - 1] == s.roles[i]) ||
                (s.roles[i] == Role::WallGuard && !guardSpawn(s.mode, s.starts[i], s.roundMap)[2]));
        }
    } else if (s.viewer + 1 < s.playerCount) {
        ++s.viewer;
        s.setupPlayer = s.viewer;
        s.selectedRole = Role::Sniper;
        s.phase = Phase::Cover;
        invalidate();
        return;
    }
    int first = (s.round - 1) % s.playerCount;
    s.game = std::make_shared<Game>(s.starts, s.roles, s.mode, first, s.roundMap, teamCount(),
                                    s.formations, s.playerCount, s.teams);
    s.viewer = s.bot ? 0 : first;
    s.coverSetup = false;
    resetChoice();
    if (s.bot)
        afterLocal();
    else {
        s.phase = Phase::Cover;
        invalidate();
    }
} // 每方两次布阵完成后才公开阵容。

bool botStep() {
    if (s.online || !s.bot || !s.game || s.game->finished() || inputPlayer(s.game->state()) == 0 ||
        s.page != Page::Battle || presentationBusy() || s.inspected >= 0 || s.pendingLeave ||
        s.playbackOptions)
        return false;
    auto action = s.tutorial
                      ? tutorialEnemyAction()
                      : chooseAction(s.game->view(inputPlayer(s.game->state())), s.rng, s.botStyle);
    if (!s.game->act(action))
        return false;
    afterLocal();
    return true;
} // 电脑同样等待上一项行动的全部文字和特效。

void syncNetwork() {
    if (!s.net)
        return;
    if (s.page == Page::Replay) {
        if (s.replayPrivate) {
            if (auto full = s.net->replay()) {
                s.last = full;
                s.lastOnline = true;
                s.replayPrivate = false;
                s.replayViews.clear();
                s.replay = std::min(s.replay, int(full->events().size()));
                s.net->saveRecovery(recoveryFile());
            }
        }
        invalidate();
        return;
    }
    s.viewer = s.net->player();
    s.mode = s.net->mode();
    s.teamSize = s.net->teamSize();
    s.playerCount = s.net->playerCount();
    s.teams = s.net->teams();
    if (auto replay = s.net->takeCompletedReplay()) {
        s.last = replay;
        s.lastOnline = true;
    }
    auto phase = s.net->phase();
    if (phase == forest::Phase::Setup && s.phase != Phase::Setup) {
        clearPresentation();
        changePage(Page::Battle);
        s.phase = Phase::Setup;
        s.starts = {};
        s.formations = {};
        s.setupPiece = 0;
        s.setupSlot = 0;
        s.selectedRole = Role::Sniper;
        s.choice.selectPiece(0);
    } else if (phase == forest::Phase::Playing || phase == forest::Phase::Waiting) {
        changePage(Page::Battle);
        s.phase = phase == forest::Phase::Playing ? Phase::Playing : Phase::Waiting;
    } else if (phase == forest::Phase::Finished) {
        s.phase = Phase::Finished;
        changePage(Page::Battle);
    } else if (phase == forest::Phase::Recovering) {
        s.phase = Phase::Error;
        changePage(Page::Battle);
    } else if (phase == forest::Phase::Aborted) {
        s.net->saveRecovery(recoveryFile());
        s.last = s.net->replay();
        s.lastOnline = true;
        s.phase = Phase::Error;
        changePage(Page::Battle);
    } else if (phase == forest::Phase::Error) {
        s.phase = Phase::Error;
        if (s.page != Page::Network)
            changePage(Page::Battle);
    }
    if (s.net->hasView()) {
        if (s.savedRevision != s.net->view().revision || s.savedAuthority != s.net->authority()) {
            s.choice.clearPlan();
            s.net->saveRecovery(recoveryFile());
            s.savedRevision = s.net->view().revision;
            s.savedAuthority = s.net->authority();
        }
        syncPresentation(s.net->view());
    }
    resetChoice();
    invalidate();
} // 状态版本变化后清理旧的未提交目标。

void connectNetwork(bool host) {
    if (!s.net || !s.window)
        return;
    wchar_t ports[16]{}, address[64]{};
    GetWindowTextW(s.portEdit, ports, 16);
    GetWindowTextW(s.addressEdit, address, 64);
    std::wstring p = ports;
    if (p.empty() || p.size() > 5 || p.find_first_not_of(L"0123456789") != std::wstring::npos) {
        s.notice = L"端口应为1–65535。";
        return;
    }
    unsigned port = 0;
    for (wchar_t c : p)
        port = port * 10 + unsigned(c - L'0');
    if (!port || port > 65535) {
        s.notice = L"端口应为1–65535。";
        return;
    }
    std::string ip;
    for (wchar_t c : std::wstring(address)) {
        if (c > 127) {
            s.notice = L"请输入IPv4地址。";
            return;
        }
        ip += char(c);
    }
    s.phase = Phase::Waiting;
    s.savedRevision = -1;
    s.savedAuthority = -1;
    if (host)
        s.net->host((unsigned short)port, s.mode, selectedMap(), teamCount(), s.playerCount,
                    s.teams);
    else
        s.net->join(ip, (unsigned short)port);
    SetTimer(s.window, 2, 50, nullptr);
    syncNetwork();
} // 网络连接方式保持与旧版相同。

void submit(Action a) {
    if (!battleInputReady())
        return;
    if (!validate(liveView(), a, &s.notice))
        return;
    if (s.online) {
        if (s.net->act(a)) {
            s.choice.clearPlan();
            syncNetwork();
        }
    } else if (s.game->act(a)) {
        tutorialProgress(a);
        afterLocal();
    }
} // 当前步执行后不可撤销。

std::string utf8(const std::wstring &value) {
    int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), nullptr, 0, nullptr,
                                nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), out.data(), n, nullptr,
                        nullptr);
    return out;
} // 导出兼容中文。

void exportReplay() {
    if (!s.last)
        return;
    try {
        wchar_t exe[32768]{};
        GetModuleFileNameW(nullptr, exe, 32768);
        auto folder = std::filesystem::path(exe).parent_path() / L"replays";
        std::filesystem::create_directories(folder);
        auto path = folder / (L"mist-v102-" + std::to_wstring(GetTickCount64()) + L".txt");
        std::ofstream file(path, std::ios::binary);
        file << "\xEF\xBB\xBF" << utf8(L"雾隐之森 · 单 / 双角色完整复盘\n");
        for (std::size_t i = 0; i < s.last->snapshots().size(); ++i) {
            const auto &snap = s.last->snapshots()[i];
            std::wstring line = L"\n步骤" + std::to_wstring(i) + L"（完成回合" +
                                std::to_wstring(snap.turns) + L"）\n";
            if (i) {
                const auto &e = s.last->events()[i - 1];
                line += e.publicText + L"\n" + e.privateText + L"\n" + e.playerNotes[0] +
                        e.playerNotes[1] + e.playerNotes[2] + e.playerNotes[3];
            }
            for (int j = 0; j < snap.playerCount * 2; ++j)
                if (j % 2 < snap.teamSize) {
                    line += playerName(j / 2) + L" " + stats(snap.units[j]) + L" " +
                            coordinate(snap.mode, snap.units[j].position) + L"\n";
                    if (snap.units[j].role == Role::WallGuard)
                        for (int k = 0; k < 3; ++k)
                            line += L"  士兵" + std::to_wstring(k + 1) + L" " +
                                    coordinate(snap.mode, snap.units[j].soldiers[k]) + L" 生命" +
                                    halfPoints(snap.units[j].soldierHealth[k]) + L"\n";
                }
            file << utf8(line);
        }
        file.flush();
        s.notice = file ? L"已保存到程序旁的replays文件夹。" : L"保存失败。";
    } catch (const std::exception &) {
        s.notice = L"保存失败，请检查目录权限。";
    }
} // 只导出终局全知记录。
