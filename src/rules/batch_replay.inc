bool Game::act(const Action &a) {
    return actCore(a);
}

// Trial actions append their own snapshots; immutable old snapshots are never cloned.
Game::Game(const Game &source, Trial) : state_(source.state_), events_(source.events_) {}

bool Game::previewBatch(const std::vector<Action> &actions, int player,
                        View &result) const { // 预选只预测己方资源，绝不提前探测隐藏目标。
    if (player < 0 || player >= state_.playerCount)
        return false;
    View original = view(player);
    Game trial(*this, Trial{});
    for (int id = 0; id < 8; ++id)
        if (id / 2 != player) {
            trial.state_.units[id].position = 0;
            trial.state_.units[id].soldiers = {};
        }
    if (!actions.empty() && !trial.runBatch(actions, nullptr))
        return false;
    result = trial.view(player);
    for (int id = 0; id < 8; ++id)
        if (id / 2 != player)
            result.units[id] = original.units[id];
    result.possible = original.possible;
    result.notices = original.notices;
    result.result = original.result;
    result.turnReport = original.turnReport;
    result.previousReport = original.previousReport;
    result.skillFeedback = original.skillFeedback;
    result.enemyAttacks = original.enemyAttacks;
    result.intel = original.intel;
    for (int id = player * 2; id < player * 2 + 2; ++id)
        result.units[id].report = original.units[id].report;
    return true;
}

bool Game::actBatch(const std::vector<Action> &actions,
                    std::wstring *why) { // 预校验整批，失败不执行任何一步。
    if (actions.empty() || actions.size() > 16)
        return false;
    Game trial(*this, Trial{});
    if (!trial.runBatch(actions, why))
        return false;
    snapshots_.insert(snapshots_.end(), std::make_move_iterator(trial.snapshots_.begin()),
                      std::make_move_iterator(trial.snapshots_.end()));
    events_ = std::move(trial.events_);
    state_ = std::move(trial.state_);
    return true;
}

bool Game::canActBatch(const std::vector<Action> &actions) const {
    if (actions.empty() || actions.size() > 16)
        return false;
    Game trial(*this, Trial{});
    return trial.runBatch(actions, nullptr);
}

bool Game::runBatch(const std::vector<Action> &actions, std::wstring *why) {
    if (actions.empty() || actions.size() > 16)
        return false;
    int player = state_.turn, unit = actions.front().unit;
    for (const auto &a : actions) {
        if (state_.turn != player || finished())
            break;
        if (a.unit != unit || !validate(view(player), a, why) || !act(a))
            return false;
        if (pendingFollowup(view(player)))
            break;
    }
    return true;
}

View Game::historicalView(int player,
                          std::size_t step) const { // 以当时状态和当时记录生成私密复盘。
    if (step >= snapshots_.size()) {
        throw std::out_of_range("replay step");
    }
    return viewAt(snapshots_[step], step, player);
} // 平摊自动结算，不再等待防守者弹窗。

const Snapshot &Game::state() const {
    return state_;
}

const std::vector<Snapshot> &Game::snapshots() const {
    return snapshots_;
}

const std::vector<Event> &Game::events() const {
    return events_;
}

bool Game::finished() const {
    return state_.winner >= 0;
} // 只读访问完整裁判。
