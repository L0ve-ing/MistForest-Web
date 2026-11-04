#pragma once
#include "actor_motion.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace mist::presentation::sprites {
using Time = std::uint64_t;
enum class Animation { Idle, Walk, Special };

struct Clip {
    int resource = 0, frames = 0;
    std::array<int, 16> durations{};
    int groundY = 120;

    int duration(int frame) const {
        return std::max(1, durations[frame]);
    }
};

using Set = std::array<Clip, 3>;

struct Bounds {
    int left, top, right, bottom;
};

struct Frame {
    Animation animation = Animation::Idle;
    int index = 0;
    int horizontalDirection = 0;
};

// Cosmetic state only: this RNG never consumes the referee / AI random stream.
class Actor {
  public:
    Frame sample(Time now, int identity, bool moving, const Set &clips, std::uint32_t seed,
                 int horizontalDirection = 0) {
        if (identity_ != identity) {
            *this = Actor{};
            identity_ = identity;
            random_ = seed ? seed : 1;
            nextSpecial_ = nextGap();
            last_ = now;
        }
        Time elapsed = now >= last_ ? now - last_ : 0;
        last_ = now;
        if (horizontalDirection)
            direction_ = horizontalDirection;
        if (moving_ != moving) {
            // Movement takes priority, cancels a cosmetic special, and starts a fresh rest.
            moving_ = moving;
            frame_ = moving ? Frame{Animation::Walk, 0} : Frame{};
            within_ = cycles_ = 0;
            nextSpecial_ = nextGap();
            elapsed = 0;
        }
        if (!clips[int(frame_.animation)].frames)
            frame_ = {};
        while (clips[int(frame_.animation)].frames) {
            const auto &clip = clips[int(frame_.animation)];
            Time remaining = Time(clip.duration(frame_.index)) - within_;
            if (elapsed < remaining) {
                within_ += elapsed;
                break;
            }
            elapsed -= remaining;
            within_ = 0;
            if (++frame_.index < clip.frames)
                continue;
            frame_.index = 0;
            if (frame_.animation == Animation::Idle && !moving_) {
                ++cycles_;
                if (clips[int(Animation::Special)].frames && cycles_ >= nextSpecial_)
                    frame_.animation = Animation::Special;
            } else if (frame_.animation == Animation::Special) {
                frame_ = {};
                cycles_ = 0;
                nextSpecial_ = nextGap();
            }
        }
        return {frame_.animation, frame_.index, direction_};
    }

    void pause(Time now) {
        last_ = now;
    }

    int completedIdleCycles() const {
        return cycles_;
    }

    int nextSpecialAfter() const {
        return nextSpecial_;
    }

  private:
    int nextGap() {
        random_ ^= random_ << 13;
        random_ ^= random_ >> 17;
        random_ ^= random_ << 5;
        return 3 + int(random_ % 8); // Never within two idle loops; no later than loop ten.
    }

    int identity_ = -1, cycles_ = 0, nextSpecial_ = 3;
    int direction_ = 0;
    std::uint32_t random_ = 1;
    bool moving_ = false;
    Time last_ = 0, within_ = 0;
    Frame frame_;
};

class Timeline {
  public:
    // Called on every page paint, including paused / covered pages. Hidden time is excluded.
    void clock(Time now, bool active) {
        if (!clockReady_) {
            seed_ = std::uint32_t(now) ^ 0x9e3779b9u;
            clockReady_ = true;
        } else if (active && active_ && now >= lastClock_) {
            time_ += now - lastClock_;
        }
        lastClock_ = now;
        active_ = active;
    }

    void beginFrame() {
        seen_.fill(false);
    }

    Frame sample(int id, int identity, bool moving, const Set &clips, int horizontalDirection = 0) {
        if (id < 0 || id >= int(actors_.size()))
            return {};
        seen_[id] = true;
        return actors_[id].sample(time_, identity, moving, clips,
                                  seed_ ^ (0x85ebca6bu * std::uint32_t(id + 1)),
                                  horizontalDirection);
    }

    void endFrame() {
        for (std::size_t i = 0; i < actors_.size(); ++i)
            if (!seen_[i])
                actors_[i].pause(time_);
    }

    Time time() const {
        return time_;
    }

  private:
    std::array<Actor, 24> actors_{}; // Eight units, up to three independently animated soldiers.
    std::array<bool, 24> seen_{};
    Time time_ = 0, lastClock_ = 0;
    std::uint32_t seed_ = 1;
    bool clockReady_ = false, active_ = false;
};

} // namespace mist::presentation::sprites
