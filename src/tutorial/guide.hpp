#pragma once
#include "../game.hpp"
#include <memory>

namespace mist::tutorial {
enum class Observation { None, PreviousLog, CurrentLog };
enum class LessonGoal { Observe, Move, Hit, SameCell, Mechanism, End };
enum class GuideStage { Explain, Operate, Feedback };

struct LessonTask {
    Action action;
    LessonGoal goal = LessonGoal::Mechanism;
    std::wstring title, explain, result;
    int observeControl = 0;
    bool independent = false;
};

struct Lesson {
    std::wstring title;
    int start = 13, enemy = 14;
    std::vector<Action> setup;
    std::vector<LessonTask> tasks;
    bool reveal = true, enemyWalk = false;
    Role enemyRole = Role::Heavy;
    bool enemyStrike = false;
};

struct GuideState {
    std::vector<Lesson> lessons;
    int lesson = 0, task = 0;
    GuideStage stage = GuideStage::Explain;
    bool basics = false, passed = false, demo = false, complete = false, actionChosen = false;
    std::shared_ptr<const Game> checkpoint;
    int completedTasks = 0;
    std::wstring outcome;
};

enum class Next { None, Changed, Retry, Lesson, Task, Complete, Leave };

class Guide {
  public:
    const GuideState &state() const {
        return state_;
    }

    const LessonTask &currentTask() const;
    int totalTasks() const;
    void reset(Role role, bool basics);
    std::shared_ptr<Game> startLesson(int index = -1);
    bool inputAllowed() const;
    void chooseAction(bool chosen);
    void recordAction(const Game &game, const Action &action);
    void observe(Observation control, bool selectedAction);
    bool canContinue() const;
    Next continueWith(const Game &game);
    std::shared_ptr<Game> retry();
    bool beginDemo();
    Action enemyAction(const View &view) const;

  private:
    GuideState state_;
    Role role_ = Role::Wolf;
};
} // namespace mist::tutorial
