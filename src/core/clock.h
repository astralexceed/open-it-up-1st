// Fixed 240 Hz game clock, kept in step with the music.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace piu {

// Converts a wall clock in milliseconds into fixed 240 Hz steps. Ticks are
// derived from the absolute time, so rounding errors cannot accumulate.
class FixedClock {
 public:
  static constexpr int kHz = 240;
  static constexpr std::int64_t kMaxCatchUp = kHz / 4;
  static constexpr std::int64_t kNoCatchUpLimit = kHz * 3600;

  // Begins counting so that now_ms corresponds to song_ms.
  void start(double now_ms, double song_ms) {
    origin_ms_ = now_ms - song_ms;
    done_ = 0;
    alpha_ = 0.0;
  }

  // Follows the audio position, as the original followed the CD. Small errors
  // are absorbed gradually so that coarse audio clocks do not cause jitter.
  void resync(double now_ms, double song_ms) {
    const double error = song_ms - (now_ms - origin_ms_);
    origin_ms_ -= std::fabs(error) > kSnapMs ? error : error * kSlew;
  }

  // Returns how many steps are due at now_ms.
  int advance(double now_ms, std::int64_t max_catch_up = kMaxCatchUp) {
    double elapsed = ticks_since_origin(now_ms);
    const std::int64_t behind = static_cast<std::int64_t>(std::floor(elapsed)) - done_;
    if (behind > max_catch_up) {
      // The page was suspended: skip the gap instead of replaying it.
      origin_ms_ += static_cast<double>(behind - max_catch_up) * 1000.0 / kHz;
      elapsed = ticks_since_origin(now_ms);
    }
    const std::int64_t target = static_cast<std::int64_t>(std::floor(elapsed));
    const int due = static_cast<int>(std::max<std::int64_t>(0, target - done_));
    done_ += due;
    alpha_ = std::clamp(elapsed - static_cast<double>(done_), 0.0, 1.0);
    return due;
  }

  // Tick containing an event timestamp from the same clock as advance().
  std::int64_t tick_at(double ms) const {
    return static_cast<std::int64_t>(std::floor(ticks_since_origin(ms)));
  }

  // Position of the song at an event timestamp from the same clock.
  double song_ms_at(double ms) const { return ms - origin_ms_; }

  // Fraction of a tick elapsed since the last step, for smooth drawing.
  double alpha() const { return alpha_; }
  std::int64_t ticks() const { return done_; }

 private:
  static constexpr double kSnapMs = 120.0;
  static constexpr double kSlew = 0.05;

  double ticks_since_origin(double ms) const { return (ms - origin_ms_) * kHz / 1000.0; }

  double origin_ms_ = 0.0;
  std::int64_t done_ = 0;
  double alpha_ = 0.0;
};

}  // namespace piu
