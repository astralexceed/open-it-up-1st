// Keeps the game at 60 frames per second on any display.

#pragma once

// Uncomment this line to let the game draw above 60 frames per second. The game
// was built for 60, and things tied to drawn frames, like the blinking, will change.
// #define PIU_UNLOCK_FPS

namespace piu {

// Decides when the next frame may be drawn, so no display can show more than 60 per second.
class FrameLimiter {
 public:
  bool due(double now_ms) {
#ifdef PIU_UNLOCK_FPS
    (void)now_ms;
    return true;
#else
    if (now_ms < next_ms_ - kToleranceMs) return false;
    next_ms_ += kFrameMs;
    // After a pause, start counting again instead of rushing to catch up.
    if (now_ms > next_ms_ + kFrameMs) next_ms_ = now_ms + kFrameMs;
    return true;
#endif
  }

 private:
  static constexpr double kFrameMs = 1000.0 / 60.0;
  // A 60 Hz display does not arrive at exactly 16.67 ms; without this it would drop to 30.
  static constexpr double kToleranceMs = 2.0;

  double next_ms_ = 0.0;
};

// How many display refreshes each frame lasts. On a display that is a multiple of
// 60 Hz, waiting for that many keeps every frame the same length on screen. Other
// rates, like 144 Hz, cannot divide 60 evenly, so they keep waiting for one.
constexpr int display_swap_interval(int refresh_hz) {
#ifdef PIU_UNLOCK_FPS
  (void)refresh_hz;
  return 1;
#else
  const int multiple = (refresh_hz + 30) / 60;
  const int difference = refresh_hz - multiple * 60;
  return multiple >= 1 && difference >= -1 && difference <= 1 ? multiple : 1;
#endif
}

}  // namespace piu
