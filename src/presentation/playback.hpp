#pragma once
#include "skill_fx.hpp"
#include "actor_animation.hpp"

namespace mist::presentation {
struct PlaybackPhase {
    std::size_t end = 0;
    Mask attacks{};
};

struct PlaybackState {
    std::vector<Notice> lines;
    std::vector<View> frames;
    std::size_t shown = 0;
    int viewer = -1, revision = -1, enemyTurn = -1, activeRevision = -1;
    std::vector<PlaybackPhase> phases;
    std::size_t actionBegin = 0, actionEnd = 0, phaseIndex = 0;
    Tick startedAt = 0, endsAt = 0, nextAt = 0, motionEndsAt = 0;
    double rate = 1;
    Tick fxDuration = SkillFxDuration;
    bool running = false;
    Tick fxAt = 0, hurtAt = 0;
    bool fxLive = false, hurtLive = false;
    Mask enemySeen{}, active{};
    SkillFx effect;
    std::array<Tick, 24> actorHitAt{};
    std::array<bool, 24> actorHit{}, actorDefeated{};
}; // 每次行动先播报，再同步显示特效与反馈，完整停留后交接。

// Consumes committed, viewer-filtered data only. Preview views belong to action
// selection and must never be submitted here. All times use the same millisecond clock.
class Playback {
  public:
    void reset();
    bool busy() const;
    // History contains filtered frames for this viewer. Missing revisions fall
    // back to committed, preserving the existing recovery/display behavior.
    void sync(const View &committed, const std::vector<View> &history, Tick now);
    bool advance(Tick now);
    void shiftTime(Tick elapsed);
    double progress(Tick now) const;
    double movementProgress(Tick now) const;

    void setSpeed(int percent) {
        speed_ = percent == 80 || percent == 125 ? percent : 100;
    }

    const View *beforeFrame() const;
    const View *afterFrame() const;
    sprites::Pose actorPose(int unit, int piece, Tick now, bool full = true) const;
    // Null when idle or when the requested historical revision is unavailable.
    // The pointer remains valid until the next mutating call on this Playback.
    const View *displayedFrame() const;

    const PlaybackState &state() const {
        return state_;
    }

    std::vector<sprites::Movement> movements() const;

  private:
    PlaybackState state_;
    int speed_ = 100;
    void applyNotice(const Notice &line);
    Tick actionDuration(std::size_t begin, std::size_t end) const;
    void startAction(Tick now);
    bool viewerHit(const PlaybackPhase &phase, int revision) const;
};
} // namespace mist::presentation
