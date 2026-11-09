// One song being played: rules, scoring, life, endings and options.

#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <vector>

#include "core/assets.h"
#include "core/sound.h"
#include "core/bga_player.h"
#include "core/coins.h"
#include "core/credit_hud.h"
#include "core/draw_list.h"
#include "core/scene.h"
#include "core/song_data.h"

namespace piu {

inline constexpr int kPanels = 10;

// Values match the result codes of the original game.
enum class Judgment { kNone = 0, kPerfect = 1, kGreat = 2, kGood = 3, kPressMiss = 16, kLateMiss = 32 };

struct InputEvent {
  std::int64_t tick;
  int panel;
  bool down;
};

// Option word of one player, with the bit values of the original game.
namespace option_bits {
inline constexpr std::uint32_t kModeMask = 0x000f;
inline constexpr std::uint32_t kModeDouble = 4;
inline constexpr std::uint32_t kSpeedMask = 0x0070;
inline constexpr std::uint32_t kVanish = 0x0100;
inline constexpr std::uint32_t kMirror = 0x0200;
inline constexpr std::uint32_t kBattle = 0x0400;
inline constexpr std::uint32_t kCopy = 0x1000;
inline constexpr std::uint32_t kCopyReversed = 0x2000;
inline constexpr std::uint32_t kRandom = 0x4000;
inline constexpr std::uint32_t kSpecialMask = 0x7f00;
}  // namespace option_bits

struct GameOptions {
  // Speed, Vanish, Mirror and the rest, one word per player.
  std::array<std::uint32_t, 2> option_words{};
  // Seed of the Random modifier.
  std::uint32_t random_seed = 1;
  int margin = 0;
  // Stage shown at the top: 1, 2, or 3 for the final stage.
  int stage = 1;
  // Row of the original life table, 0 to 19.
  int life_level = 0;
  // A Double chart: one player uses both pads, in one centered playfield.
  bool double_play = false;
  // Replaces the BGA with the 3D flight when set.
  const Scene* flight = nullptr;
  // Machine difficulty setting: 0 easy, 1 normal, 2 hard.
  int machine_difficulty = 0;
  // STAGE BREAK setting: 0 to 2. A stage can be failed from stage_break + 1 on.
  int stage_break = 1;
  const CoinBox* coins = nullptr;
  // The machine plays by itself.
  bool demo = false;
  // Player 2 alone plays a single chart on the right pad.
  bool right_side_only = false;
};

// Values match the end-of-song states of the original game.
enum class LifeState : std::uint8_t { kPlaying = 0, kFailed = 1, kBelowLine = 2, kCleared = 3, kAbsent = 4 };

struct PlayerStats {
  float life = 15.0f;
  bool danger = false;
  int late_misses = 0;
  LifeState state = LifeState::kPlaying;
  int points = 0;
  int combo = 0;
  int max_combo = 0;
  int miss_run = 0;
  // Rows ended as Perfect, Great, Good, Bad and Miss, in that order.
  std::array<int, 5> tally{};
  Judgment last = Judgment::kNone;
};

// Registers every file a song needs with the asset tracker.
void request_song_assets(const Bga& bga, Assets& assets);

// One song being played: BGA, scrolling arrows, judging, effects and score.
class Game {
 public:
  Game(Chart chart, const Bga& bga, const Bga& gameview, const Assets& assets, GameOptions options);
  // Resolved layers point into bga_, which survives a move but not a copy.
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;
  Game(Game&&) = default;
  Game& operator=(Game&&) = default;

  // Assets the song named but the platform could not supply.
  const std::vector<std::string>& missing() const { return missing_; }
  bool has_arrows() const { return has_arrows_; }

  // Events must be queued in the order they happened.
  void queue_input(InputEvent event);
  // Advances one 240 Hz step. The original's frame logic runs every 4 steps.
  void step();
  void draw(DrawList& out, double alpha) const;
  // Draws gameview.bga over another screen, as the DANCE GRADE closing does.
  void draw_overlay(DrawList& out, float time, float brightness) const;
  bool finished() const;
  // True once the original would stop the CD for the ending; read clears it.
  bool take_audio_stop();
  SoundQueue& sounds() { return sounds_; }

  int players() const { return options_.double_play ? 1 : 2; }
  bool uses_flight() const { return options_.flight != nullptr; }
  bool is_battle() const { return (options_.option_words[1] & option_bits::kBattle) != 0; }
  // Frame of gameview.bga the ending is showing, for the test suite.
  int ending_time() const { return ending_time_; }
  bool uses_player(int player) const { return uses_lane_[static_cast<std::size_t>(player)]; }
  const PlayerStats& stats(int player) const { return stats_[static_cast<std::size_t>(player)]; }
  // Final letter: 'S', 'A', 'B', 'C', 'D' or 'F'.
  char grade(int player) const;

 private:
  SoundQueue sounds_;
  // Advances once per frame drawn.
  mutable int drawn_frames_ = 0;
  struct Row {
    ChartRow chart;
    std::array<Judgment, 2> judged{};
    // Battle: the rival stepped here on time, so this row can be missed.
    std::array<bool, 2> armed{};
  };

  // Press mask, 5 columns or 10 in Double.
  struct Pad {
    std::uint16_t pending = 0;
    std::uint16_t active = 0;
    int last_row = -1;
    bool latched = false;
  };

  enum class ReceptorState : std::uint8_t { kIdle = 0, kPressed = 1, kHit = 2 };

  struct ReceptorFx {
    ReceptorState state = ReceptorState::kIdle;
    int timer = 0;
  };

  using ReceptorSet = std::array<ReceptorFx, kPanels>;

  struct JudgmentFx {
    bool active = false;
    Judgment result = Judgment::kNone;
    int timer = 0;
    int combo = 0;
  };

  struct TileSetup {
    int cell_w, cell_h, tile_w, tile_h;
  };

  enum class Ending : std::uint8_t { kNone, kFail, kBelowLine, kClear };

  void resolve_hud(const Assets& assets);
  void apply_input();
  void run_frame();
  void update_rows(double now);
  void autoplay_rows(double now);
  void judge(Row& row, int lane, int y);
  void prepare_battle();
  void update_battle_phase();
  void battle_step(Row& row, int lane, int y);
  void award(int lane, Judgment result, std::uint16_t mask);
  int logic_y(double row_time, double now, std::size_t lane) const;
  double row_y(double row_time, double now, std::size_t lane) const;
  int columns() const { return options_.double_play ? 10 : 5; }
  float column_x(int lane, int column) const;
  void update_life(int lane, Judgment result);
  void update_end_states();
  void update_ending();
  LifeState lane_state(std::size_t lane) const;
  void draw_bga(DrawList& out, double seconds) const;
  void draw_life_bar(DrawList& out, int lane) const;
  void draw_icons(DrawList& out, int lane) const;
  void draw_stage(DrawList& out) const;
  void draw_credits(DrawList& out) const;
  void draw_demo_label(DrawList& out) const;
  void draw_sprite_at(DrawList& out, int id, float x, float y) const;
  void draw_receptors(DrawList& out) const;
  void draw_arrows(DrawList& out, double seconds) const;
  void draw_judgment(DrawList& out, int lane) const;
  void draw_score(DrawList& out, int lane) const;
  void draw_arrow_tile(DrawList& out, int sheet, int frame, float x, float y, float w, float h, int turn,
                       float alpha = 1.0f) const;
  void apply_modifiers();
  void draw_sprite(DrawList& out, int id, float scale, float offset_y, Color color) const;
  struct ResolvedQuad {
    Texture texture;
    float x, y, w, h, u0, v0, u1, v1;
  };

  const ResolvedQuad* hud_sprite(int id) const;

  Chart chart_;
  Bga bga_;
  Bga gameview_;
  GameOptions options_;
  std::vector<Row> rows_;
  BgaPlayer layers_;
  BgaPlayer gameview_layers_;
  CreditHud credit_hud_;
  std::mt19937 random_;
  std::vector<std::string> missing_;
  std::array<Texture, 8> arrow_textures_{};
  bool has_arrows_ = false;
  std::array<bool, 2> uses_lane_{};
  // Battle. Phase 1: both step freely on white arrows.
  // Phase 2: both play what the rival sent. Phase 0: nothing is on the field yet.
  bool battle_ = false;
  int battle_offset_ = 0;
  int battle_blocks_ = 0;
  int battle_block_rows_ = 0;
  int battle_phase_ = 0;
  int last_step_row_ = 0;
  std::array<int, 2> lane_rows_{};
  std::array<double, 2> pixels_per_second_{};
  std::vector<ResolvedQuad> hud_quads_;
  Texture combo_digits_;
  Texture score_digits_;

  std::deque<InputEvent> pending_;
  std::array<Pad, 2> pads_{};
  int poll_count_ = 0;
  std::array<ReceptorSet, 2> receptors_{};
  std::array<ReceptorSet, 2> drawn_receptors_{};
  std::array<JudgmentFx, 2> judgment_{};
  std::array<JudgmentFx, 2> drawn_judgment_{};
  int flight_counter_ = 0;
  int danger_frames_ = 0;
  int grace_rows_ = 0;
  int flicker_frame_ = 0;
  Ending ending_ = Ending::kNone;
  int ending_phase_ = 0;
  int ending_counter_ = 0;
  int ending_time_ = 0;
  float ending_brightness_ = 1.0f;
  bool hide_playfield_ = false;
  bool blackout_ = false;
  bool ended_ = false;
  bool audio_stop_ = false;
  int beat_number_ = 0;
  int beat_flash_ = 0;
  std::int64_t tick_ = 0;
  std::array<PlayerStats, 2> stats_{};
};

}  // namespace piu
