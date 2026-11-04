#pragma once
#include "game.hpp"

namespace mist::selection {
enum class Modifier { Rush, Ignite, Lunge, IncludeSelf };

struct SelectionState {
    int unit = 0, selected = 0, subject = 2, piece = 0, amount = 1;
    int extra = 0, bloomLanding = 0, enemyPiece = 0;
    Kind kind = Kind::Attack;
    bool batching = false, rush = false, ignite = false, lunge = false;
    bool swordIncludesSelf = false;
    std::vector<Action> planned;
};

// Owns an uncommitted choice. No window, network session, clock or referee state.
// Views may be batch previews; only the caller submits actions against live state.
class ActionSelection {
  public:
    const SelectionState &state() const { return state_; }
    void reset(const View &view, int preferredUnit = -1);
    void clearTargets();
    bool selectUnit(const View &live, int unit);
    void selectKind(const View &view, Kind kind);
    // Cell picking includes two-cell walls, rose/landing/target and valid batch steps.
    bool selectCell(const View &preview, int cell);
    // Setup screens may select a cell before any game/View exists.
    void pointAt(int cell);
    void selectSubject(int unit);
    void selectPiece(int piece);
    void cycleSubjectPiece();
    void selectAmount(int amount);
    int amountForSlot(const View &view, int slot) const;
    void toggle(Modifier modifier);
    void setBatching(bool enabled);
    void clearPlan();
    Action action(const View &view, int target) const;
    bool canKind(const View &view, Kind kind) const;
    bool parameterAvailable(const View &view, int amount) const;

  private:
    SelectionState state_;
};
} // namespace mist::selection
