// Mode and song selection, input codes and the help screen.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/assets.h"
#include "core/bga_player.h"
#include "core/coins.h"
#include "core/credit_hud.h"
#include "core/draw_list.h"
#include "core/sound.h"
#include "core/stage_cfg.h"

namespace piu {

inline constexpr int kModeEasy = 0;
inline constexpr int kModeHard = 1;
inline constexpr int kModeDouble = 2;
inline constexpr int kModeNonstop = 3;
inline constexpr int kModeBattle = 4;
inline constexpr int kModeEvent = 5;

// What survives from one stage of a credit to the next.
struct SelectSession {
  int players = 0;  // Bit 0 is player 1, bit 1 is player 2.
  int stage = 0;    // 1 to 3.
  int mode = 0;
  int previous_song = 9;
  std::array<std::uint32_t, 2> option_words{};
};

// The mode and CD screens, and the title card that follows.
class SelectScreen {
 public:
  enum class Event { kPlayModeMusic, kPlaySelectMusic, kPreview, kStopMusic, kNeedTitle, kDone };

  SelectScreen(const StageConfig& config, const Bga& mode_bga, const Bga& music_bga, const Bga& cd_bga,
               const Assets& assets, CoinBox& coins, SelectSession& session, bool event_mode);

  // A panel went down: 0-4 on the left pad, 5-9 on the right.
  void press(int panel);
  void step();
  void draw(DrawList& out) const;
  std::optional<Event> take_event();

  // The track to play after kPlayModeMusic, kPlaySelectMusic or kPreview.
  int track() const { return track_; }
  // Valid after kNeedTitle and kDone.
  const StageSong& song() const { return songs_[static_cast<std::size_t>(selected_song_)]; }
  const StageTrack& song_track() const;
  // The platform loaded the title sprite named by song_track().
  void title_ready(const Assets& assets);

  SoundQueue& sounds() { return sounds_; }

 private:
  void tick_seconds();
  SoundQueue sounds_;
  int ticked_seconds_ = 0;
  enum class Phase { kMode, kMusic, kWaitTitle, kTitle, kDone };

  struct TitleQuad {
    Texture texture;
    float x, y, w, h, u0, v0, u1, v1;
  };

  void enter_mode();
  void enter_music();
  void run_loop();
  void update_mode();
  void update_music();
  void update_join();
  void join_pressing_players();
  void update_codes();
  void apply_code(std::size_t player, const std::string& code);
  void draw_mode(DrawList& out) const;
  void draw_music(DrawList& out) const;
  void draw_icons(DrawList& out) const;
  void draw_time(DrawList& out) const;
  bool pressed(int panel) const { return pressed_[static_cast<std::size_t>(panel)]; }
  void choose_song(int slot);
  int seconds_left() const;

  const StageConfig& config_;
  CoinBox& coins_;
  SelectSession& session_;
  BgaPlayer mode_;
  BgaPlayer music_;
  BgaPlayer cds_;
  CreditHud hud_;
  Texture digits_;
  std::vector<std::string> missing_;

  Phase phase_ = Phase::kMode;
  std::vector<Event> events_;
  std::int64_t ticks_ = 0;
  int frames_60_ = 0;
  int t_ = 0;
  int confirm_t_ = 0;
  bool confirming_ = false;
  int previous_ = 8;
  int selected_ = 9;
  int timer_start_ = 0;
  int last_seconds_ = 0;
  int track_ = 0;
  std::array<int, 4> slots_{};
  std::vector<StageSong> songs_;
  int selected_song_ = 0;
  std::array<bool, 10> pressed_{};
  std::array<std::string, 2> codes_;
  std::vector<TitleQuad> title_;
  int title_frames_ = 0;
};

// The tutorial shown before the first selection.
class HelpScreen {
 public:
  HelpScreen(const Bga& bga, const Assets& assets, const CoinBox& coins, int players);
  void press_center(int player);
  void step();
  void draw(DrawList& out) const;
  bool finished() const { return state_ == 2; }

  SoundQueue& sounds() { return sounds_; }

 private:
  SoundQueue sounds_;
  void run_frame();

  const CoinBox& coins_;
  BgaPlayer bga_;
  CreditHud hud_;
  std::vector<std::string> missing_;
  int players_ = 0;
  std::int64_t ticks_ = 0;
  int timer_ = 0;
  int frame_ = 0;
  int state_ = 0;
  float brightness_ = 1.0f;
  std::array<bool, 2> pressed_{};
};

// Requests everything the two screens need up front.
void request_select_assets(const Bga& mode_bga, const Bga& music_bga, const Bga& cd_bga, const Bga& help_bga, Assets& assets);

}  // namespace piu
