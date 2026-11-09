// The DANCE GRADE results screen.

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "core/assets.h"
#include "core/draw_list.h"
#include "core/sound.h"

namespace piu {

struct ResultPlayer {
  bool active = false;
  // Perfect, Great, Good, Bad and Miss, then Max Combo.
  std::array<int, 6> counts{};
  int points = 0;
  char grade = 'F';
};

// The DANCE GRADE screen shown after a cleared song.
class ResultsScreen {
 public:
  // With game_over, the screen closes with its fade.
  // With battle, the letters and their sounds give way to WIN and LOSE.
  ResultsScreen(const std::array<ResultPlayer, 2>& players, const Assets& assets, bool game_over = false,
                bool battle = false);

  // A player pressed the center panel: skips the count-up, as in the original.
  void press_center(int player);
  // Advances one 240 Hz step; the screen updates every 4 steps.
  void step();
  void draw(DrawList& out) const;
  bool finished() const { return phase_ == Phase::kDone; }
  SoundQueue& sounds() { return sounds_; }
  // True while gameview.bga has to be drawn over the screen.
  bool closing() const { return phase_ == Phase::kClosing; }
  float overlay_time() const { return static_cast<float>(overlay_time_); }
  float overlay_brightness() const { return overlay_brightness_; }

 private:
  enum class Phase { kCounting, kHold, kFinal, kClosing, kDone };

  struct Quad {
    Texture texture;
    float x, y, w, h, u0, v0, u1, v1;
  };

  void run_frame();
  void play_letter();
  void draw_sprite(DrawList& out, int id, Color color) const;
  void draw_row(DrawList& out, int row, Color color, bool final_values) const;

  SoundQueue sounds_;
  std::array<ResultPlayer, 2> players_;
  bool letter_played_ = false;
  bool battle_ = false;
  std::vector<Quad> quads_;
  Texture digits_;
  Phase phase_ = Phase::kCounting;
  std::int64_t ticks_ = 0;
  int phase_frames_ = 0;
  bool skip_ = false;
  bool game_over_ = false;
  float screen_brightness_ = 1.0f;
  float fade_ = 1.0f;
  int overlay_time_ = 0xf0;
  float overlay_brightness_ = 1.0f;
  // Count-up state per row.
  std::array<bool, 6> started_{};
  std::array<int, 6> start_time_{};
  std::array<std::array<int, 2>, 6> shown_{};
};

}  // namespace piu
