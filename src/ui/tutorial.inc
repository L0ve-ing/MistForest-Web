// Offline training adapts the portable guide to window, selection and playback operations.
int tutorialControl(Kind kind);

int tutorialObservationControl(int control) {
    using mist::tutorial::Observation;
    return control == int(Observation::PreviousLog)  ? LogPrevious
           : control == int(Observation::CurrentLog) ? LogCurrent
                                                     : 0;
}

const LessonTask &currentTask() {
    return s.guide.currentTask();
}

bool tutorialInputAllowed() {
    return s.guide.inputAllowed();
}

void trainingSelection() {
    s.guide.chooseAction(s.game && pendingFollowup(s.game->view(0)));
    s.choice = mist::selection::ActionSelection{};
    if (s.game) {
        auto v = s.game->view(0);
        s.choice.reset(v, 0);
        s.choice.selectKind(v, pendingFollowup(v) ? (v.budget.swordAdvance ? Kind::SwordAdvance
                                                                           : Kind::FlameAdvance)
                                                  : Kind::End);
    }
}

void startLesson(int index = -1) {
    if (s.window)
        KillTimer(s.window, 1);
    s.game = s.guide.startLesson(index);
    const auto &g = s.guide.state();
    const auto &l = g.lessons.at(g.lesson);
    s.starts = {l.start, l.start, l.enemy, l.enemy};
    s.roles = {s.selectedRole, s.selectedRole, l.enemyRole, l.enemyRole};
    s.formations = {};
    if (s.selectedRole == Role::WallGuard)
        s.formations[0] = {l.start, 12, 18};
    s.phase = s.game->finished()             ? Phase::Finished
              : inputPlayer(s.game->state()) ? Phase::Waiting
                                             : Phase::Playing;
    clearPresentation();
    syncPresentation(liveView(), 0);
    while (presentationBusy())
        advancePresentation(s.playback.state().nextAt);
    s.historyTurn = -1;
    s.historyOffset = 0;
    s.historyMeasuredTurn = -2;
    trainingSelection();
    invalidate();
}

void launchTutorial(Role role, bool basics) {
    if (!navigate(Page::Battle))
        return;
    s.online = s.networkMode = s.series = false;
    s.choice.setBatching(false);
    s.bot = s.tutorial = true;
    s.choice.clearPlan();
    s.selectedRole = basics ? Role::Wolf : role;
    s.playerCount = 2;
    s.teamSize = 1;
    s.teams = false;
    s.mode = Mode::Heroes;
    s.roundMap = Map{};
    s.viewer = 0;
    s.coverSetup = false;
    s.guide.reset(s.selectedRole, basics);
    startLesson();
}

void tutorialProgress(const Action &action) {
    if (s.tutorial && !s.online)
        s.guide.recordAction(*s.game, action);
}

void tutorialObserved(int id) {
    using mist::tutorial::Observation;
    auto observation = id == LogPrevious  ? Observation::PreviousLog
                       : id == LogCurrent ? Observation::CurrentLog
                                          : Observation::None;
    s.guide.observe(observation,
                    id == tutorialControl(currentTask().action.kind) || id == Move || id == Attack);
}

bool tutorialCanContinue() {
    const auto &g = s.guide.state();
    return !presentationBusy() && s.inspected < 0 && !s.pendingLeave && s.guide.canContinue() &&
           (g.stage != GuideStage::Feedback || g.complete || s.phase == Phase::Playing ||
            s.phase == Phase::Finished);
}

void tutorialContinue() {
    if (!s.tutorial || !tutorialCanContinue())
        return;
    using mist::tutorial::Next;
    switch (s.guide.continueWith(*s.game)) {
    case Next::Leave:
        command(TutorialNext);
        return;
    case Next::Retry:
        tutorialRetry();
        return;
    case Next::Lesson:
        startLesson();
        return;
    case Next::Task:
        trainingSelection();
        break;
    default:
        break;
    }
    invalidate();
}

void tutorialRetry() {
    if (!s.tutorial || presentationBusy() || s.inspected >= 0 || s.pendingLeave)
        return;
    if (s.guide.state().complete) {
        launchTutorial(s.selectedRole, s.guide.state().basics);
        return;
    }
    auto restored = s.guide.retry();
    if (!restored)
        return;
    if (s.window)
        KillTimer(s.window, 1);
    s.game = std::move(restored);
    s.phase = Phase::Playing;
    clearPresentation();
    syncPresentation(liveView(), 0);
    while (presentationBusy())
        advancePresentation(s.playback.state().nextAt);
    trainingSelection();
    s.historyTurn = -1;
    s.historyOffset = 0;
    invalidate();
}

void tutorialDemo() {
    if (!s.tutorial || s.online || presentationBusy() || s.guide.state().complete ||
        s.inspected >= 0 || s.pendingLeave)
        return;
    tutorialRetry();
    if (s.guide.beginDemo())
        submit(currentTask().action);
    else
        invalidate();
}

Action tutorialEnemyAction() {
    return s.guide.enemyAction(s.game->view(1));
}

int tutorialControl(Kind k) {
    switch (k) {
    case Kind::Move:
        return Move;
    case Kind::Attack:
    case Kind::AttackRush:
        return Attack;
    case Kind::Dash:
        return Dash;
    case Kind::ShadowMode:
        return currentTask().action.amount == 1 ? ShadowLeadMode : ShadowFollowMode;
    case Kind::Follow:
        return Follow;
    case Kind::Lead:
        return Lead;
    case Kind::Charge:
        return Charge;
    case Kind::Transform:
        return Transform;
    case Kind::Leap:
        return Leap;
    case Kind::Hunt:
        return Hunt;
    case Kind::Silence:
        return Silence;
    case Kind::Cleave:
        return Cleave;
    case Kind::Retreat:
        return Retreat;
    case Kind::Blood:
        return Blood;
    case Kind::ArmorUp:
        return ArmorUp;
    case Kind::ArmorDown:
        return ArmorDown;
    case Kind::Thrust:
        return Thrust;
    case Kind::Flame:
        return Flame;
    case Kind::FireStep:
        return FireStep;
    case Kind::FlameAdvance:
        return FlameAdvance;
    case Kind::BuildWall:
        return WallBuild;
    case Kind::PlantRose:
        return PlantRose;
    case Kind::Bloom:
        return Bloom;
    case Kind::SwordStab:
        return SwordStab;
    case Kind::ThrowSword:
        return ThrowSword;
    case Kind::RecallSword:
        return RecallSword;
    case Kind::SwordAdvance:
        return SwordAdvance;
    case Kind::End:
        return EndTurn;
    default:
        return Attack;
    }
}

std::wstring tutorialActionName(Kind k) {
    switch (k) {
    case Kind::Move:
        return L"移动";
    case Kind::Attack:
    case Kind::AttackRush:
        return s.selectedRole == Role::Wolf     ? L"撕咬"
               : s.selectedRole == Role::Sniper ? L"绝影"
                                                : L"攻击";
    case Kind::Dash:
        return L"冲刺";
    case Kind::ShadowMode:
        return currentTask().action.amount == 1 ? L"主动带动" : L"指定跟随模式";
    case Kind::Follow:
        return L"指定跟随";
    case Kind::Lead:
        return L"带动移动";
    case Kind::Charge:
        return L"冲锋";
    case Kind::Transform:
        return currentTask().action.target ? L"切换狼形" : L"切换人形";
    case Kind::Leap:
        return L"基础日字";
    case Kind::Hunt:
        return L"普通攻击·倾轧";
    case Kind::Silence:
        return L"①沉默与易伤";
    case Kind::Cleave:
        return L"②挥劈";
    case Kind::Retreat:
        return L"③秘密跳跃";
    case Kind::Blood:
        return L"嗜血·探测";
    case Kind::ArmorUp:
        return L"负甲";
    case Kind::ArmorDown:
        return L"卸甲";
    case Kind::Thrust:
        return L"直线刺击";
    case Kind::Flame:
        return L"喷射火焰";
    case Kind::FireStep:
        return L"火步";
    case Kind::FlameAdvance:
        return L"进入火焰";
    case Kind::BuildWall:
        return L"建长城";
    case Kind::PlantRose:
        return L"玫瑰之吻";
    case Kind::Bloom:
        return L"荆棘盛放";
    case Kind::SwordStab:
        return L"剑刺";
    case Kind::ThrowSword:
        return L"飞剑";
    case Kind::RecallSword:
        return L"召回飞剑";
    case Kind::SwordAdvance:
        return L"进入攻击格";
    case Kind::End:
        return L"结束回合";
    default:
        return L"行动";
    }
}

int tutorialPointer() {
    if (s.guide.state().complete)
        return TutorialContinue;
    const auto &t = currentTask();
    if (s.guide.state().stage != GuideStage::Operate)
        return TutorialContinue;
    if (t.goal == LessonGoal::Observe)
        return tutorialObservationControl(t.observeControl)
                   ? tutorialObservationControl(t.observeControl)
                   : TutorialContinue;
    if (s.choice.state().piece != t.action.piece)
        return Piece0 + t.action.piece;
    if (t.action.kind == Kind::AttackRush && !s.choice.state().rush)
        return RushToggle;
    if (t.action.kind == Kind::FireStep && bool(t.action.amount) != s.choice.state().ignite)
        return IgniteToggle;
    if (s.choice.state().kind != t.action.kind)
        return tutorialControl(t.action.kind);
    if (t.action.kind == Kind::SwordStab &&
        s.choice.state().swordIncludesSelf != bool(t.action.amount))
        return IncludeSelf;
    if (t.independent)
        return s.choice.state().selected || targetlessKind(t.action.kind)
                   ? Commit
                   : tutorialControl(t.action.kind);
    if ((t.action.kind == Kind::Flame || t.action.kind == Kind::ArmorDown) &&
        s.choice.state().amount != t.action.amount)
        return Amount1 + t.action.amount - 1;
    if (t.action.kind == Kind::Bloom) {
        if (!s.choice.state().extra)
            return CellBase + t.action.extra;
        if (!s.choice.state().bloomLanding)
            return CellBase + t.action.landing;
    }
    if (!targetlessKind(t.action.kind) && t.action.target &&
        s.choice.state().selected != t.action.target)
        return CellBase + t.action.target;
    return Commit;
}

void tutorialArrow(HDC dc, int id) {
    auto it = std::find_if(s.hits.begin(), s.hits.end(),
                           [&](const Hit &h) { return h.id == id && h.enabled; });
    if (it == s.hits.end())
        return;
    RECT r = it->rect;
#ifdef _WIN32
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    Gdiplus::Pen halo(Gdiplus::Color(230, 205, 153, 45), 2.5f);
    g.DrawRectangle(&halo, float(r.left - 3), float(r.top - 3), float(r.right - r.left + 6),
                    float(r.bottom - r.top + 6));
    float tx = (r.left + r.right) / 2.f, ty = r.top - 8.f,
          sx = tx < 450    ? 490.f
               : tx > 1020 ? 984.f
                           : 735.f,
          sy = 383.f;
    if (id == TutorialContinue) {
        sx = 985;
        sy = 331;
        tx = r.left - 8.f;
        ty = (r.top + r.bottom) / 2.f;
    }
    Gdiplus::Pen pen(Gdiplus::Color(245, 184, 126, 25), 3.f);
    Gdiplus::AdjustableArrowCap arrow(5, 6, true);
    pen.SetCustomEndCap(&arrow);
    pen.SetStartCap(Gdiplus::LineCapRound);
    g.DrawBezier(&pen, sx, sy, sx + (tx - sx) * .65f, sy + 28.f, tx, ty - 42.f, tx, ty);
#else
    HPEN pen = CreatePen(PS_SOLID, 3, GOLD);
    auto old = SelectObject(dc, pen);
    MoveToEx(dc, (r.left + r.right) / 2, r.top - 20, nullptr);
    LineTo(dc, (r.left + r.right) / 2, r.top - 4);
    SelectObject(dc, old);
    DeleteObject(pen);
#endif
}

void tutorialBattle(HDC dc) {
    const auto &g = s.guide.state();
    syncPresentation(liveView());
    auto v = displayedView();
    const auto &l = g.lessons[g.lesson];
    const auto &t = currentTask();
    bool busy = presentationBusy(), operate = battleInputReady(),
         choose = g.stage == GuideStage::Operate && !busy && s.phase == Phase::Playing;
    feedPanel(dc);
    card(dc, rect(450, 122, 570, 773));
    card(dc, rect(1040, 122, 446, 773));
    text(dc, g.basics ? L"新手教学 · 一步一步学" : roleName(s.selectedRole) + L" · 角色练习",
         rect(470, 142, 530, 37), 25, INK, true);
    card(dc, rect(470, 193, 530, 182), RGB(255, 249, 224), GOLD, 16, 2);
    text(dc,
         L"第 " + std::to_wstring(g.lesson + 1) + L" / " + std::to_wstring(g.lessons.size()) +
             L" 课 · 步骤 " + std::to_wstring(g.task + 1) + L" / " +
             std::to_wstring(l.tasks.size()),
         rect(486, 204, 498, 21), 13, MUTED);
    text(dc, g.complete ? L"练习完成" : t.title + (g.demo ? L" · 示范" : L""),
         rect(486, 232, 498, 30), 21, TEAL, true);
    std::wstring help =
        g.complete ? L"已经学会操作与关键机制。实际对局敌方站位隐藏，可以随时按H查看己方技能。"
        : g.stage == GuideStage::Feedback
            ? (busy ? L"先观看同步的文字、斩击与结果，播放完再继续。" : g.outcome)
            : t.explain;
    text(dc, help, rect(486, 271, 498, 92), 15, INK);
    auto units = v.units;
    bool reveal = l.reveal && !s.online;
    if (reveal) {
        int revision = s.playback.state().running ? std::max(0, s.playback.state().activeRevision)
                                                  : int(s.game->snapshots().size()) - 1;
        revision = std::min(revision, int(s.game->snapshots().size()) - 1);
        units = s.game->snapshots()[revision].units;
    }
    board(dc, s.mode, s.roundMap, units, reveal);
    text(dc,
         reveal ? L"练习场公开陪练位置 · 正式对局隐藏敌方站位"
                : L"隐藏信息练习 · 对手站位不显示 · 红框仅为攻击格",
         rect(470, 843, 530, 30), 14, MUTED);
    characterAvatar(dc, rect(1064, 144, 54, 54), v.units[0].role, v.units[0].form, true, true);
    text(dc, stats(v.units[0]), rect(1130, 145, 327, 49), 20, INK, true);
    text(dc, statusText(v), rect(1064, 209, 394, 61), 15, MUTED);
    text(dc, l.title, rect(1064, 286, 394, 31), 21, TEAL, true);
    int y = 330;
    std::vector<Kind> kinds;
    if (!g.complete) {
        kinds.push_back(t.action.kind);
        if (g.basics && (t.goal == LessonGoal::Move || t.goal == LessonGoal::Hit)) {
            kinds = {Kind::Move, Kind::Attack};
        }
    }
    if (t.goal != LessonGoal::Observe && !g.complete) {
        for (auto k : kinds) {
            bool available = choose && canKind(liveView(), k);
            button(dc, tutorialControl(k), tutorialActionName(k), rect(1064, y, 394, 44),
                   s.choice.state().kind == k, available, 19);
            y += 56;
        }
        if (s.selectedRole == Role::WallGuard) {
            for (int k = 0; k < 3; ++k)
                button(dc, Piece0 + k, L"士兵" + std::to_wstring(k + 1),
                       rect(1064 + k * 134, y, 126, 32), s.choice.state().piece == k, choose, 15);
            y += 43;
        }
        if (t.action.kind == Kind::AttackRush) {
            button(dc, RushToggle, s.choice.state().rush ? L"攻击突进：开" : L"攻击突进：关",
                   rect(1064, y, 394, 32), s.choice.state().rush, choose);
            y += 42;
        }
        if (t.action.kind == Kind::Flame || t.action.kind == Kind::ArmorDown) {
            for (int k = 0; k < 3; ++k)
                button(dc, Amount1 + k, std::to_wstring(k + 1), rect(1064 + k * 134, y, 126, 32),
                       s.choice.state().amount == k + 1,
                       operate && parameterAvailable(liveView(), k + 1));
            y += 43;
        }
        if (t.action.kind == Kind::FireStep) {
            button(dc, IgniteToggle, s.choice.state().ignite ? L"点燃终点：开" : L"点燃终点：关",
                   rect(1064, y, 394, 32), s.choice.state().ignite, choose);
            y += 42;
        }
        if (t.action.kind == Kind::SwordStab) {
            button(dc, IncludeSelf,
                   s.choice.state().swordIncludesSelf ? L"包含自身：开" : L"包含自身：关",
                   rect(1064, y, 394, 32), s.choice.state().swordIncludesSelf, choose);
            y += 42;
        }
        if (t.action.kind == Kind::ThrowSword) {
            text(dc, L"飞剑按距离支付剑气；原地留剑为0。", rect(1064, y, 394, 32), 15);
            y += 42;
        }
        if (t.action.kind != Kind::End && t.action.kind != Kind::ShadowMode) {
            button(dc, Commit, L"执行 · 空格", rect(1064, y, 394, 48), true,
                   operate &&
                       validate(liveView(), selectedAction(liveView(), s.choice.state().selected)),
                   20);
            y += 61;
        }
    }
    if (!g.complete)
        text(dc,
             g.stage == GuideStage::Explain   ? L"先读上方提示，点击「开始本步」。"
             : g.stage == GuideStage::Operate ? L"按金框和箭头操作，选格后再执行。"
             : busy                           ? L"反馈播放中…"
             : g.passed                       ? L"目标已达成。读完结果再继续。"
                                              : L"目标未达成，点击「重试本步」。",
             rect(1064, std::max(y, 536), 394, 53), 16, MUTED);
    button(dc, TutorialContinue,
           g.complete                       ? L"选择其他角色"
           : g.stage == GuideStage::Explain ? L"开始本步"
           : g.stage == GuideStage::Operate ? L"我看明白了"
           : g.demo                         ? L"自己试一次"
                                            : L"下一步",
           rect(1064, 622, 394, 48), true, tutorialCanContinue(), 20);
    button(dc, TutorialDemo, L"观看示范", rect(1064, 686, 190, 40), false, !busy && !g.complete);
    button(dc, TutorialRetry, g.complete ? L"重新练习" : L"重试本步", rect(1268, 686, 190, 40),
           false, !busy);
    button(dc, TutorialNext, L"选择其他角色教程", rect(1064, 741, 394, 39));
    text(dc, L"H 查看完整技能 · 空格执行\nW 移动 · A / D / F 浏览战报", rect(1064, 798, 394, 58),
         15, MUTED);
    if (!busy && !g.complete)
        tutorialArrow(dc, tutorialPointer());
    footer(dc, L"实操达成目标后继续 · 示范不计完成进度 · 重试恢复本步起点");
}
