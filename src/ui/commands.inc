void command(int id) {
    if (id == MoreActions && s.page == Page::Battle && !s.pendingLeave && s.inspected < 0 &&
        !s.playbackOptions) {
        s.moreActions = !s.moreActions;
        invalidate();
        return;
    }
    if (s.moreActions) {
        s.moreActions = false;
        if (id != Surrender) {
            invalidate();
            return;
        }
    }
    if (s.playbackOptions) {
        if (id == ClosePlaybackOptions)
            closePlaybackOptions();
        else if (id == AnimationFull || id == AnimationSimple)
            s.fullAnimation = id == AnimationFull;
        else if (id == SpeedSlow || id == SpeedNormal || id == SpeedFast)
            s.playbackSpeed = id == SpeedSlow ? 80 : id == SpeedFast ? 125 : 100;
        invalidate();
        return;
    }
    if (id == PlaybackOptions && !s.pendingLeave && s.inspected < 0) {
        s.optionsAt = visualNow();
        s.playbackOptions = true;
        invalidate();
        return;
    }
    if (s.pendingLeave) {
        if (id == LeaveCancel || id == LeaveConfirm) {
            int original = s.pendingLeave;
            auto elapsed = visualNow() - s.leaveAt;
            s.pendingLeave = 0;
            s.playback.shiftTime(elapsed);
            s.replayActors.shiftTime(elapsed);
            s.replayFxAt += elapsed;
            if (id == LeaveConfirm) {
                s.allowLeave = true;
                command(original);
                s.allowLeave = false;
            }
            invalidate();
        }
        return;
    }
    s.currentCommand = id; // 所有按钮共用规则校验，不让快捷键绕过权限。
    if (s.page == Page::Battle) {
        if (s.inspected >= 0) {
            if (id == CloseDetails)
                closeDetails();
            return;
        }
        if ((id >= DetailsBase && id < DetailsBase + 8) || id == SelfSkills ||
            (id == Skills && hasView())) {
            if (hasView()) {
                auto v = currentView();
                int unit = id >= DetailsBase ? id - DetailsBase : s.choice.state().unit;
                if (unit >= 0 && unit < v.playerCount * 2 && unit % 2 < v.teamSize) {
                    s.inspected = unit;
                    s.inspectAt = GetTickCount64();
                    invalidate();
                }
            }
            return;
        }
        bool readOnly = id == LogCurrent || id == LogPrevious || id == LogNext || id == Home ||
                        id == Review || id == Online || id == GiveUp || id == ReturnBattle ||
                        id == TutorialNext;
        if (presentationBusy() && !readOnly)
            return; // 鼠标、键盘、过期命中区和直接提交都受相同播放锁约束。
    }
    if (s.tutorial && s.page == Page::Battle) {
        if (id == TutorialContinue) {
            tutorialContinue();
            return;
        }
        if (id == TutorialRetry) {
            tutorialRetry();
            return;
        }
        if (id == TutorialDemo) {
            tutorialDemo();
            return;
        }
        tutorialObserved(id);
    }
    if (s.page == Page::Battle && hasView() && pendingFollowup(liveView())) {
        bool cell = id > CellBase && id <= CellBase + 81;
        bool navigation = id == Home || id == Skills || id == Review || id == LogCurrent ||
                          id == LogPrevious || id == LogNext || id == Surrender ||
                          id == TutorialNext;
        if (id == EndTurn || id == StayAfterAttack) {
            submit({s.choice.state().unit,
                    liveView().budget.swordAdvance ? Kind::SwordAdvance : Kind::FlameAdvance, 0});
            return;
        }
        if (!cell && !navigation && id != Commit)
            return;
    }
    if (id >= TargetBase && id < TargetBase + 8) {
        if (hasView() && currentView().units[id - TargetBase].health > 0) {
            s.choice.selectSubject(id - TargetBase);
        }
        invalidate();
        return;
    } // 附骨目标按钮只显示职业名，不展示数值。
    if (id > CellBase && id <= CellBase + 81) {
        int cell = id - CellBase;
        if (s.page == Page::Editor)
            changeMapCell(cell);
        else if (s.page == Page::Battle && (s.phase == Phase::Setup || s.phase == Phase::Playing)) {
            if (s.phase == Phase::Setup && s.selectedRole == Role::WallGuard) {
                int who = s.viewer * 2 + s.setupSlot;
                s.formations[who][s.setupPiece] = cell;
                s.starts[who] = s.formations[who][0];
                if (s.setupPiece < 2)
                    ++s.setupPiece;
                s.choice.pointAt(s.formations[who][s.setupPiece]);
            } else if (s.phase == Phase::Playing)
                s.choice.selectCell(currentView(), cell);
            else
                s.choice.pointAt(cell);
        }
        invalidate();
        return;
    } // 点击只选下一步目标，不提前执行。
    if (s.page == Page::Editor && ((id >= HorizontalBase && id < HorizontalBase + 90) ||
                                   (id >= VerticalBase && id < VerticalBase + 90))) {
        bool h = id < VerticalBase;
        int i = id - (h ? HorizontalBase : VerticalBase);
        if (s.editor.document.paintEdge(h, i))
            s.editor.notice.clear();
        invalidate();
        return;
    } // 格边工具与格子地形分开。
    if (id >= RoleBase && id < RoleBase + 13) {
        Role role = Role(id - RoleBase);
        if (s.page == Page::Skills ||
            (s.phase == Phase::Setup && (s.setupSlot == 0 || s.roles[s.viewer * 2] != role))) {
            if (s.page != Page::Skills && s.selectedRole != role) {
                s.formations[s.viewer * 2 + s.setupSlot] = {};
                s.starts[s.viewer * 2 + s.setupSlot] = 0;
                s.setupPiece = 0;
                s.choice.pointAt(0);
            }
            s.selectedRole = role;
        }
        invalidate();
        return;
    } // 阵容禁止同职业，技能页可自由浏览。
    if (actionSelectionCommand(id)) {
        invalidate();
        return;
    }
    if (mapEditorCommand(id)) {
        invalidate();
        return;
    }
    switch (id) { // 按钮编号映射到单一行为。
    case ShadowLeadMode:
        submit({s.choice.state().unit, Kind::ShadowMode, 0, -1, 0, 0, 1});
        break;
    case ShadowFollowMode:
        submit({s.choice.state().unit, Kind::ShadowMode, 0, -1, 0, 0, 2});
        break;
    case TutorialBasics:
        launchTutorial(Role::Wolf, true);
        break;
    case TutorialRole:
        launchTutorial(s.selectedRole, false);
        break;
    case TutorialContinue:
        tutorialContinue();
        break;
    case TutorialDemo:
        tutorialDemo();
        break;
    case TutorialRetry:
        tutorialRetry();
        break;
    case TutorialNext:
        if (s.window)
            KillTimer(s.window, 1);
        s.tutorial = false;
        changePage(Page::Skills);
        break;
    case NetResume:
        resumeNetwork();
        break;
    case GiveUp:
        if (s.net)
            s.net->finishRecovery();
        syncNetwork();
        break;
    case ReturnBattle:
        s.replayPrivate = false;
        s.replayViews.clear();
        changePage(Page::Battle);
        syncNetwork();
        break;
    case Surrender:
        if (hasView() && s.phase == Phase::Playing &&
            (!s.window || MessageBoxW(s.window, L"确认投降？你的全部角色会退出本局。", L"投降",
                                      MB_YESNO | MB_ICONQUESTION) == IDYES))
            submit({s.choice.state().unit, Kind::Surrender});
        break;
    case ShowAll:
        break;
    case Home:
        if (navigate(Page::Lobby))
            s.online = false;
        break;
    case Skills:
        if (navigate(Page::Skills))
            s.online = false;
        break;
    case Review:
        openReplay();
        break; // 导航离开活动对局需确认。
    case PeopleUp:
        if (!s.teams)
            s.playerCount = std::min(4, s.playerCount + 1);
        break;
    case PeopleDown:
        if (!s.teams)
            s.playerCount = std::max(2, s.playerCount - 1);
        break;
    case Teams:
        s.teams = true;
        s.playerCount = 4;
        break;
    case FreeForAll:
        s.teams = false;
        break;
    case Single:
        s.teamSize = 1;
        break;
    case Duo:
        if (heroMode(s.mode))
            s.teamSize = 2;
        break;
    case Small:
        s.mode = Mode::Scout;
        break;
    case Heroes:
        s.mode = Mode::Heroes;
        s.customEnabled = false;
        break;
    case Cpu:
    case Hotseat:
        if (navigate(Page::Config)) {
            s.bot = id == Cpu;
            s.online = false;
            s.networkMode = false;
        }
        break;
    case BotBalanced:
    case BotAggressive:
    case BotCautious:
        s.botStyle = BotStyle(id - BotBalanced);
        break;
    case Settings:
        if (navigate(Page::Config))
            s.online = false;
        break;
    case Assist:
        s.assist = !s.assist;
        break;
    case Series:
        s.series = !s.series;
        break; // 大厅配置仅保留两种地图。
    case Begin: {
        s.playerCount = s.teams ? 4 : std::clamp(s.playerCount, 2, 4);
        s.series = false;
    }
        if (s.networkMode) {
            if (navigate(Page::Network)) {
                s.online = true;
                s.game.reset();
                if (!s.net)
                    s.net = std::make_unique<forest::Session>();
                s.addresses.clear();
                for (const auto &ip : forest::localAddresses())
                    s.addresses += std::wstring(ip.begin(), ip.end()) + L"\n";
            }
        } else {
            s.round = 1;
            s.score = {};
            prepareRound();
        }
        break;
    case Ready:
        clearPresentation();
        s.phase = s.coverSetup ? Phase::Setup : Phase::Playing;
        if (!s.coverSetup)
            resetChoice();
        break; // 遮罩后才显示本人视角。
    case CycleRole:
        if (battleInputReady())
            selectBattleUnit(nextSelectableUnit(liveView()));
        break;
    case Unit0:
    case Unit1:
        selectBattleUnit(s.viewer * 2 + id - Unit0);
        break;
    case SetupPiece0:
    case SetupPiece1:
    case SetupPiece2:
        s.setupPiece = id - SetupPiece0;
        s.choice.pointAt(s.formations[s.viewer * 2 + s.setupSlot][s.setupPiece]);
        break;
    case Commit:
        if (s.phase == Phase::Setup) {
            if (s.online) {
                int i = s.viewer * 2 + s.setupSlot;
                int start = s.selectedRole == Role::WallGuard ? s.formations[i][0]
                                                              : s.choice.state().selected;
                if (!valid(s.mode, start, battleMap()) ||
                    (s.selectedRole == Role::WallGuard &&
                     !validFormation(s.mode, battleMap(), start, s.formations[i])))
                    break;
                s.starts[i] = start;
                s.roles[i] = s.selectedRole;
                if (s.selectedRole != Role::WallGuard)
                    s.formations[i] = {};
                s.choice.pointAt(0);
                s.setupPiece = 0;
                if (teamCount() == 2 && s.setupSlot == 0) {
                    s.setupSlot = 1;
                    s.selectedRole = s.roles[i] == Role::Sniper ? Role::Wolf : Role::Sniper;
                } else {
                    if (teamCount() == 1) {
                        s.starts[s.viewer * 2 + 1] = s.starts[s.viewer * 2];
                        s.roles[s.viewer * 2 + 1] = s.roles[s.viewer * 2];
                    }
                    int base = s.viewer * 2;
                    if (s.net->chooseStart({s.starts[base], s.starts[base + 1]},
                                           {s.roles[base], s.roles[base + 1]},
                                           {s.formations[base], s.formations[base + 1]}))
                        syncNetwork();
                }
            } else
                setupLocal();
        } else if (s.phase == Phase::Playing) {
            if (!s.choice.state().planned.empty())
                submitPlan();
            else
                submit(selectedAction(currentView(), s.choice.state().selected));
        }
        break; // 逐步裁定结果返回后才允许再次提交。
    case LogCurrent:
        s.historyTurn = -1;
        s.historyOffset = 0;
        s.historyMeasuredTurn = -2;
        break;
    case LogPrevious: {
        int current = s.historyTurn;
        if (current < 0 && s.playback.state().shown)
            current = s.playback.state().lines[s.playback.state().shown - 1].turn;
        int previous = -1;
        for (std::size_t i = 0; i < s.playback.state().shown; ++i)
            if (s.playback.state().lines[i].turn < current)
                previous = std::max(previous, s.playback.state().lines[i].turn);
        if (previous >= 0)
            s.historyTurn = previous;
        s.historyOffset = 0;
        s.historyMeasuredTurn = -2;
        break;
    }
    case LogNext: {
        if (s.historyTurn < 0)
            break;
        int next = 100000;
        for (std::size_t i = 0; i < s.playback.state().shown; ++i)
            if (s.playback.state().lines[i].turn > s.historyTurn)
                next = std::min(next, s.playback.state().lines[i].turn);
        s.historyTurn = next == 100000 ? -1 : next;
        s.historyOffset = 0;
        s.historyMeasuredTurn = -2;
        break;
    }
    case EndTurn:
        submit({s.choice.state().unit, Kind::End});
        break;
    case Pass:
        if (s.phase == Phase::Hold) {
            s.viewer = inputPlayer(s.game->state());
            s.coverSetup = false;
            s.phase = Phase::Cover;
            clearPresentation();
            resetChoice();
        }
        break;
    case Again:
        if (s.online) {
            if (s.net->newRound())
                syncNetwork();
        } else {
            if (!s.series || s.score[0] >= 2 || s.score[1] >= 2)
                s.score = {};
            ++s.round;
            prepareRound();
        }
        break; // 每方回合结束才切换玩家。
    case Online:
        if (navigate(Page::Config)) {
            s.online = false;
            s.networkMode = true;
            s.bot = false;
        }
        break;
    case NetHost:
        connectNetwork(true);
        break;
    case NetJoin:
        connectNetwork(false);
        break;
    case NetCancel:
        if (s.net)
            s.net->close();
        changePage(Page::Config);
        s.online = false;
        break; // 联机只更改本会话。
    case MapEdit:
        if (navigate(Page::Editor)) {
            s.online = false;
            s.editor.document.begin(s.customMap);
            syncDimensions();
        }
        break;
    case ReplayPrev:
        selectReplayStep(std::max(0, s.replay - 1));
        break;
    case ReplayNext:
        selectReplayStep(std::min(int(s.replayViews.size() ? s.replayViews.size() - 1
                                      : s.last             ? s.last->events().size()
                                                           : 0),
                                  s.replay + 1));
        break;
    case Export:
        exportAnyReplay();
        break;
    default:
        break; // 复盘只在合法时间轴范围内操作。
    } // 命令完成。
    invalidate(); // 重新生成当前可点击操作。
} // 命令派发结束。
