void appendNotices(View &v, const Event &e,
                   int revision) { // 只读取该观察者可见的文字与公开判定集合。
    auto first = v.notices.size();
    Mask active{};
    std::size_t start = 0;
    while (start < e.publicText.size()) {
        auto end = e.publicText.find(L'\n', start);
        auto line = e.publicText.substr(start, end == std::wstring::npos ? end : end - start);
        if (!line.empty()) {
            Mask named{};
            for (int c = 1; c <= cellCount(v.mode); ++c)
                if (e.attackCells[c - 1] && line.find(coordinate(v.mode, c)) != std::wstring::npos)
                    named[c - 1] = true;
            if (std::any_of(named.begin(), named.end(), [](bool x) { return x; }))
                active = named;
            else if (line.find(L"燃烧结算") == 0) {
                active = {};
                for (int c = 1; c <= cellCount(v.mode); ++c)
                    if (line.find(coordinate(v.mode, c)) != std::wstring::npos)
                        active[c - 1] = true;
            }
            v.notices.push_back({revision, e.turn, e.player, noticeWeight(line), line, active});
            for (const auto &cue : e.publicCues)
                if (cue.textOffset == start) {
                    v.notices.back().effect = cue.effect;
                    v.notices.back().absorbed = cue.absorbed;
                    break;
                }
        }
        if (end == std::wstring::npos)
            break;
        start = end + 1;
    } // 连续多格攻击逐格配合结果；冲锋集合在同一条中显示，不标顺序。
    if (e.player == v.player && !e.privateText.empty() && e.feedback.empty()) {
        v.notices.push_back({revision, e.turn, e.player, 0, L"仅己方：" + e.privateText, {}});
    } // 本人的秘密移动和技能反馈只进入本人队列。
    if (!e.playerNotes[v.player].empty() && (e.feedback.empty() || e.player != v.player)) {
        v.notices.push_back(
            {revision, e.turn, e.player, 1, L"私人反馈：" + e.playerNotes[v.player], {}});
    } // 被施法者保留自己的受控反馈，不读取施法者探测结果。
    if (e.player == v.player && !e.feedback.empty()) {
        v.notices.push_back({revision, e.turn, e.player, 2, L"仅己方：" + e.feedback, {}});
    } // 嗜血和沉默探测使用重点大字提示。
    for (std::size_t i = first; i < v.notices.size(); ++i) {
        auto &n = v.notices[i];
        n.unit = e.unit;
        n.kind = e.action.kind;
        n.feedback = i > first || n.text.find(L"仅己方：") == 0 || n.text.find(L"私人反馈：") == 0;
        if (n.text.find(L"仅己方：") == 0 || n.text.find(L"私人反馈：") == 0)
            n.attacks = active;
    } // 私人结果沿用本次攻击格；同一行动中的后续判定独立同步。
} // 提示拆分结束。

View Game::view(int player) const {
    return viewAt(state_, events_.size(), player);
}

View Game::viewAt(const Snapshot &snapshot, std::size_t eventCount, int player) const { // 裁判向玩家开放自己的两名角色及公开信息。
    auto historyEnd = events_.begin() + eventCount;
    auto historyReverse = std::make_reverse_iterator(historyEnd);
    View v;
    v.roundDamage = snapshot.roundDamage;
    v.roundNumber = snapshot.roundNumber;
    v.roundMask = snapshot.roundMask;
    v.playerCount = snapshot.playerCount;
    v.teams = snapshot.teams;
    v.eliminated = snapshot.eliminated;
    v.playerTurns = snapshot.playerTurns;
    v.teamSize = snapshot.teamSize;
    v.mode = snapshot.mode;
    v.map = snapshot.map;
    v.buildings = snapshot.buildings;
    v.fires = snapshot.fires;
    v.rubble = snapshot.rubble;
    v.roses = snapshot.roses;
    v.swords = snapshot.swords;
    v.intel = snapshot.intel[player];
    v.units = snapshot.units;
    v.player = player;
    v.turn = snapshot.turn;
    v.winner = snapshot.winner;
    v.turns = snapshot.turns;
    v.revision = snapshot.revision;
    v.budget = snapshot.budget;
    v.reaction = snapshot.reaction;
    if (v.reaction.unit >= 0 && v.reaction.unit / 2 != player) {
        v.reaction.mask = 0;
        v.reaction.health = {};
    }
    v.possible = snapshot.possible[player]; // 复制后立刻抹除敌方私人信息。
    if (player != v.turn)
        v.budget = Turn{}; // 非行动方不可由预算判断移动方向或秘密锚点。
    for (int i = 0; i < 8; ++i)
        if (i / 2 != player) {
            auto &u = v.units[i];
            u.position = 0;
            u.soldiers = {};
            u.soldierSilenced = {};
            u.soldierVulnerable = {};
            u.soldierControlSource = {{-1, -1, -1}};
            u.soldierControlExpires = {};
            u.vulnerable = false;
            u.controlSource = -1;
            u.controlExpires = 0;
            u.soldierGuard = {};
            u.soldierWhirl = {};
            u.followPiece = 0;
            u.ammoBlocked = false;
            u.silenced = false;
            u.guard = false;
            u.whirlImmune = false;
            u.follow = -1;
            u.sense = -2;
            u.report.clear();
        } // 人形隐狼的被控状态也不泄露给施法者。
    int enemyTurn = -1, ownTurn = -1;
    for (auto it = historyReverse; it != events_.rend(); ++it) {
        if (it->player != player) {
            if (enemyTurn < 0)
                enemyTurn = it->turn;
            if (it->turn == enemyTurn)
                for (int c = 0; c < 81; ++c)
                    v.enemyAttacks[c] = v.enemyAttacks[c] || it->attackCells[c];
        } else {
            if (ownTurn < 0)
                ownTurn = it->turn;
            if (it->turn == ownTurn && v.skillFeedback.empty() && !it->feedback.empty())
                v.skillFeedback = it->feedback;
        }
    }
    v.enemyAttackTurn = enemyTurn;
    if (eventCount != 0) {
        for (std::size_t i = 0; i < eventCount; ++i)
            if (events_[i].turn >= 0)
                appendNotices(v, events_[i], int(i) + 1);
    } // 保留最近敌方回合的公开攻击集合，技能反馈仅限最近本人回合。
    if (eventCount != 0) {
        v.result = events_[eventCount - 1].publicText;
        int latest = events_[eventCount - 1].turn, previous = -1;
        for (auto it = historyReverse; it != events_.rend(); ++it)
            if (it->turn < latest) {
                previous = it->turn;
                break;
            }
        for (auto event = events_.begin(); event != historyEnd; ++event) {
            const auto &e = *event;
            if (e.turn != latest && e.turn != previous)
                continue;
            auto &report = e.turn == latest ? v.turnReport : v.previousReport;
            if (report.empty())
                report = L"第" + std::to_wstring(e.turn + 1) + L"回合 · 玩家" +
                         std::to_wstring(e.player + 1) + L"\n";
            report += e.publicText + L"\n";
            if (e.player == player && !e.privateText.empty())
                report += L"  仅己方：" + e.privateText + L"\n";
            if (!e.playerNotes[player].empty())
                report += L"  私人反馈：" + e.playerNotes[player];
        }
    }
    return v; // 结果文字只取经过脱敏的公共字段。
} // 视图结束。
