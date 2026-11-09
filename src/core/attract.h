// The attract loop: logo, intro, title screen and demo songs.

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
#include "core/machine_settings.h"
#include "core/sound.h"

namespace piu {

// What the machine shows while nobody plays: Andamiro, the intro, the title
// screen and the demo, in that order and forever.
class AttractLoop {
 public:
  enum class Event { kIntroMusicStart, kIntroMusicStop, kStartDemo, kPlayersReady };
  enum class Phase { kAndamiro, kIntro, kTitle, kDemo, kDone };

  // Both Bga objects must outlive the loop.
  AttractLoop(const Bga& andamiro, const Bga& start, const Assets& assets, CoinBox& coins, const MachineSettings& settings);

  // A player pressed the center panel on the title screen.
  void press_center(int player);
  // Advances one 240 Hz step; the screens update every 4 steps.
  void step();
  void draw(DrawList& out) const;

  std::optional<Event> take_event();
  // Valid after kStartDemo: index in the list of ten demo songs.
  int demo_index() const { return demo_index_; }
  // Valid after kPlayersReady: bit 0 is player 1, bit 1 is player 2.
  int players() const { return (joined_[0] ? 1 : 0) | (joined_[1] ? 2 : 0); }
  // The demo that kStartDemo asked for is over.
  void demo_finished();
  // The credit ended: back to the Andamiro logo.
  void restart() { demo_finished(); }
  Phase phase() const { return phase_; }
  std::size_t missing_assets() const { return missing_.size(); }

  SoundQueue& sounds() { return sounds_; }

 private:
  SoundQueue sounds_;
  void run_frame();
  void run_andamiro();
  void run_intro();
  void run_title();
  void enter_intro_or_title();
  void enter_title();

  CoinBox& coins_;
  bool demo_sound_ = true;
  std::vector<std::string> missing_;
  BgaPlayer andamiro_;
  BgaPlayer start_;
  CreditHud hud_;
  Texture intro_;
  Texture white_;

  Phase phase_ = Phase::kAndamiro;
  std::vector<Event> events_;
  std::int64_t ticks_ = 0;
  int timer_ = 0;
  int frame_count_ = 0;
  float brightness_ = 1.0f;

  int intro_frame_ = 0;

  int title_time_ = 0;
  float title_fade_ = 1.0f;
  float title_brightness_ = 1.0f;
  bool title_fading_ = false;
  bool has_coin_ = false;
  std::array<bool, 2> pressed_{};
  std::array<bool, 2> joined_{};
  int join_wait_ = 0;

  int demo_index_ = 0;
  int next_demo_ = 0;
};

}  // namespace piu
