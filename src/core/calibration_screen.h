// The screen that measures how late the player hears the music.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/assets.h"
#include "core/draw_list.h"

namespace piu {

inline constexpr double kCalibrationBeatMs = 600.0;
inline constexpr int kCalibrationWarmupTaps = 4;
inline constexpr int kCalibrationMeasuredTaps = 16;
inline constexpr int kAudioOffsetMinMs = -150;
inline constexpr int kAudioOffsetMaxMs = 400;

struct TapMeasure {
  int offset_ms = 0;
  bool steady = false;
};

// Finds how far from the beat the taps fall. Each tap is its position within the beat, in ms.
TapMeasure measure_taps(const std::vector<double>& taps);

// A WAV file with the beat, made here so that it needs no game data.
std::string make_click_track();

class CalibrationScreen {
 public:
  enum class Result { kRunning, kSaved, kCancelled };

  CalibrationScreen(int current_ms, bool spanish, const Texture& atlas);

  // song_ms is the position of the music at the moment of the press.
  void tap(double song_ms);
  void cancel() { result_ = Result::kCancelled; }
  void click(float x, float y);
  void draw(DrawList& out) const;

  bool wants_audio() const { return !measured_ && result_ == Result::kRunning; }
  Result result() const { return result_; }
  int offset_ms() const { return saved_ms_; }

 private:
  const char* text(const char* en, const char* es) const { return spanish_ ? es : en; }

  int saved_ms_;
  bool spanish_;
  Texture atlas_;
  std::vector<double> taps_;
  bool measured_ = false;
  TapMeasure measure_;
  Result result_ = Result::kRunning;
};

}  // namespace piu
