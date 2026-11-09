// Gameplay: judging steps, life and score, Battle, modifiers, endings and drawing.

#include "core/game.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>
#include <set>
#include <string>
#include <utility>

#include "core/clock.h"

namespace piu {
namespace {

// Gameplay values of the original game.
constexpr float kArrowSize = 69.0f;
constexpr int kCell = 70;
constexpr int kGrid = 3;
constexpr double kHitY = 35.0;
constexpr float kReceptorY = 32.0f;
constexpr int kZoneLate = 10;
constexpr int kZoneEarly = 42;
constexpr double kBaseSpacing = 30.0;
constexpr double kBgaTicksPerSecond = 60.0;
constexpr double kPi = 3.14159265358979323846;

constexpr std::array<std::array<float, 5>, 2> kColumnX = {{{32, 80, 128, 176, 224}, {352, 400, 448, 496, 544}}};
constexpr std::array<float, 10> kDoubleColumnX = {80, 128, 176, 224, 272, 320, 368, 416, 464, 512};
constexpr std::array<int, 5> kTurn = {1, 0, 0, 3, 2};
constexpr std::array<std::array<int, 6>, 5> kFrame = {{{8, 0, 1, 0, 6, 7},
                                                       {3, 4, 5, 0, 1, 2},
                                                       {6, 7, 8, 3, 4, 5},
                                                       {3, 4, 5, 0, 1, 2},
                                                       {8, 0, 1, 0, 6, 7}}};
constexpr std::array<std::array<int, 6>, 5> kSheet = {{{2, 3, 3, 2, 2, 2},
                                                       {2, 2, 2, 2, 2, 2},
                                                       {4, 4, 4, 4, 4, 4},
                                                       {2, 2, 2, 2, 2, 2},
                                                       {2, 3, 3, 2, 2, 2}}};
constexpr std::array<std::array<int, 6>, 6> kJudgeBands = {{{30, 38, 26, 41, 24, 43},
                                                            {30, 38, 26, 40, 24, 42},
                                                            {31, 37, 27, 39, 25, 41},
                                                            {31, 36, 27, 38, 25, 40},
                                                            {32, 35, 28, 37, 26, 39},
                                                            {32, 34, 28, 36, 26, 38}}};

constexpr std::array<std::pair<int, const char*>, 6> kArrowFiles = {
    {{2, "arrow01"}, {3, "arrow02"}, {4, "arrow03"}, {5, "arrow04"}, {6, "arrow05"}, {7, "arrow06"}}};

// Frame logic and animation counters run at this many steps per update.
constexpr int kStepsPerFrame = 4;
constexpr int kBeatFlashTicks = 24;
constexpr int kReceptorFxFrames = 16;
constexpr int kJudgmentFrames = 40;
constexpr int kMaskClearPolls = 4;

constexpr std::array<int, 5> kReceptorBaseFrame = {2, 2, 6, 2, 2};
constexpr std::array<int, 5> kReceptorSheet = {3, 3, 7, 3, 3};
constexpr std::array<int, 5> kReceptorDrawOrder = {2, 0, 1, 4, 3};
constexpr float kReceptorBaseSize = 69.0f;

// Receptor scale per animation frame, for a press and for a hit.
constexpr std::array<float, 16> kPressScale = {
    0.935714f, 0.785714f, 0.721429f, 0.742857f, 0.764286f, 0.785714f, 0.807143f, 0.828571f,
    0.850000f, 0.871429f, 0.892857f, 0.914286f, 0.935714f, 0.957143f, 0.978571f, 1.000000f};
constexpr std::array<float, 16> kHitScale = {
    1.064286f, 1.214286f, 1.278571f, 1.257143f, 1.235714f, 1.214286f, 1.192857f, 1.171429f,
    1.150000f, 1.128571f, 1.107143f, 1.085714f, 1.064286f, 1.042857f, 1.021429f, 1.000000f};

// Effect tiles per frame, for a press and for a hit.
constexpr std::array<int, 16> kPressFxFrame = {4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 0, 0, 1, 1, 2, 2};
constexpr std::array<int, 16> kPressFxFrameCenter = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7};
constexpr std::array<int, 16> kPressFxSheet = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4};
constexpr std::array<int, 16> kPressFxSheetCenter = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
constexpr std::array<int, 16> kHitFxFrame = {8, 8, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6};
constexpr std::array<int, 16> kHitFxFrameCenter = {7, 7, 8, 8, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
constexpr std::array<int, 16> kHitFxSheet = {5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};
constexpr std::array<int, 16> kHitFxSheetCenter = {6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

// Judgment text scale per frame. Bad and Miss bounce.
constexpr std::array<float, 20> kTextGrowScale = {2.000000f, 1.866667f, 1.733333f, 1.600000f, 1.466667f, 1.333333f,
                                                  1.200000f, 1.066667f, 1.000000f, 1.000000f, 1.000000f, 1.000000f,
                                                  1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f,
                                                  1.000000f, 1.000000f};
constexpr std::array<float, 20> kTextBounceScale = {1.600000f, 1.466667f, 1.322222f, 1.222222f, 1.111111f, 1.000000f,
                                                    0.800000f, 0.666667f, 0.600000f, 0.700000f, 0.900000f, 1.200000f,
                                                    0.950000f, 1.100000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f,
                                                    1.000000f, 1.000000f};

// gamescr.spr entries: first judgment sprite (Perfect) and COMBO label.
constexpr int kTextSpritePlayer1 = 0x39;
constexpr int kTextSpritePlayer2 = 0x34;
constexpr int kTextSpriteDouble = 0x2f;
constexpr int kComboLabelPlayer1 = 0x2a;
constexpr int kComboLabelPlayer2 = 0x29;
constexpr int kComboLabelDouble = 0x28;
constexpr int kScoreLabelPlayer1 = 0x3b;
constexpr int kScoreLabelPlayer2 = 0x3a;
constexpr int kHudSpriteCount = 0x48;
constexpr float kComboDigitX1 = 0x56;
constexpr float kComboDigitX2 = 0x196;
constexpr float kComboDigitXDouble = 0xf6;
constexpr float kComboDigitStep = 0x2d;
constexpr float kScoreDigitX1 = 0x18;
constexpr float kScoreDigitX2 = 0x1b0;
constexpr float kScoreDigitY = 0x1a4;
constexpr float kScoreDigitStep = 0x19;

constexpr std::array<int, 4> kStageSprite = {0, 4, 3, 2};
constexpr int kAutoplayGoodPercent = 30;
constexpr double kDemoSeconds = 51.0;
constexpr double kVanishStartY = 0xe4;
constexpr float kVanishFade = 0.01f;
constexpr std::array<float, 2> kIconX = {8.0f, 0x256};
constexpr float kIconStartY = 0x6e;
constexpr float kIconStepY = 0x28;
constexpr int kLifeBaseSprite[2] = {0x47, 0x46};
constexpr int kLifeFillSprite[2] = {0x45, 0x44};

struct LifeRow {
  float combo, perfect, great, good, bad, miss, run, minimum;
};

// One row per life level.
constexpr std::array<LifeRow, 20> kLifeTable = {{
    {0.5f, 0.5f, 0.3f, 0.0f, -1.15f, -2.3f, -0.2f, -5.0f},
    {0.3f, 0.4f, 0.2f, 0.0f, -1.2f, -2.4f, -0.25f, -5.0f},
    {0.3f, 0.4f, 0.2f, 0.0f, -1.3f, -2.6f, -0.3f, -6.0f},
    {0.3f, 0.4f, 0.2f, 0.0f, -1.5f, -3.0f, -0.35f, -6.0f},
    {0.3f, 0.4f, 0.2f, 0.0f, -2.0f, -4.0f, -0.4f, -7.0f},
    {0.2f, 0.4f, 0.2f, 0.0f, -2.5f, -5.0f, -0.42f, -7.0f},
    {0.2f, 0.5f, 0.3f, 0.0f, -2.5f, -5.0f, -0.43f, -7.0f},
    {0.2f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.6f, -8.0f},
    {0.2f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.65f, -8.0f},
    {0.2f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.7f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.75f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.8f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.84f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.86f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.9f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.91f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.92f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.93f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.94f, -8.0f},
    {0.1f, 0.4f, 0.2f, 0.0f, -3.0f, -6.0f, -0.95f, -8.0f},
}};


float lerp(float a, float b, float f) { return a + (b - a) * f; }

std::string zero_padded(int value, std::size_t width) {
  std::string text = std::to_string(value);
  if (text.size() < width) text.insert(0, width - text.size(), '0');
  return text;
}

// Texture coordinates of a corner of the unit quad, turned by 90 degree steps.

}  // namespace


void request_song_assets(const Bga& bga, Assets& assets) {
  for (const auto& [sheet, file] : kArrowFiles) {
    (void)sheet;
    assets.request_texture(file);
  }
  assets.request_sprite("gamescr.spr");
  assets.request_texture("level");
  assets.request_texture("dec2");
  assets.request_texture("dec4");
  for (const BgaLayer& layer : bga.layers) {
    if (layer.name.empty() || layer.keys.empty()) continue;
    if (has_extension(layer.name, "spr")) {
      assets.request_sprite(layer.name);
    } else {
      assets.request_texture(layer.name);
    }
  }
}

Game::Game(Chart chart, const Bga& bga, const Bga& gameview, const Assets& assets, GameOptions options)
    : chart_(std::move(chart)), bga_(bga), gameview_(gameview), options_(options) {
  options_.stage = std::clamp(options_.stage, 1, 3);
  options_.life_level = std::clamp(options_.life_level, 0, 19);
  options_.margin = std::clamp(options_.margin, 0, 5);
  options_.stage_break = std::clamp(options_.stage_break, 0, 2);
  random_.seed(options_.random_seed);
  // Battle drops every option player 2 carried.
  battle_ = is_battle();
  if (battle_) options_.option_words[1] &= option_bits::kBattle;
  for (std::size_t lane = 0; lane < 2; ++lane) {
    const int speed = static_cast<int>((options_.option_words[lane] & option_bits::kSpeedMask) >> 4) + 1;
    pixels_per_second_[lane] = kBaseSpacing * speed / chart_.row_seconds;
  }

  rows_.reserve(chart_.rows.size());
  for (ChartRow row : chart_.rows) {
    if (options_.double_play) {
      row.mask = {static_cast<std::uint16_t>(row.mask[0] | (row.mask[1] << 5)), 0};
    } else if (options_.right_side_only) {
      row.mask = {0, row.mask[0]};
    }
    rows_.push_back({row, {}});
    for (std::size_t lane = 0; lane < 2; ++lane) {
      if (row.mask[lane] != 0) {
        uses_lane_[lane] = true;
        ++lane_rows_[lane];
      }
    }
  }

  if (battle_) prepare_battle();
  else apply_modifiers();

  has_arrows_ = true;
  for (const auto& [sheet, file] : kArrowFiles) {
    if (const Texture* texture = assets.texture(file)) {
      arrow_textures_[static_cast<std::size_t>(sheet)] = *texture;
    } else {
      has_arrows_ = false;
      missing_.emplace_back(file);
    }
  }
  layers_ = BgaPlayer(bga_, assets, missing_);
  gameview_layers_ = BgaPlayer(gameview_, assets, missing_);
  credit_hud_ = CreditHud(assets);
  resolve_hud(assets);

  // Player 2's state starts absent when nobody plays that side.
  for (std::size_t lane = 0; lane < 2; ++lane) {
    if (!uses_lane_[lane]) stats_[lane].state = LifeState::kAbsent;
  }
  // Danger is allowed after this many rows, by machine difficulty.
  // Rows with a step on any side: with two players it is not just the longer side.
  const auto total_rows = static_cast<int>(std::count_if(rows_.begin(), rows_.end(), [](const Row& row) {
    return row.chart.mask[0] != 0 || row.chart.mask[1] != 0;
  }));
  grace_rows_ = options_.machine_difficulty == 0 ? total_rows * 3 / 5
                : options_.machine_difficulty == 1 ? total_rows / 2
                                                   : 0;
}

void Game::prepare_battle() {
  // The rules depend on the chart name. Each attack block is sent to the next one.
  struct Rules {
    const char* name;
    int offset, blocks, block_rows;
  };
  static constexpr std::array<Rules, 4> kRules = {{{"battle1.stf", -2, 14, 0x20},
                                                   {"battle2.stf", 13, 20, 0x20},
                                                   {"battle3.stf", 13, 14, 0x40},
                                                   {"battle4.stf", -16, 14, 0x40}}};
  std::string source = chart_.source;
  std::transform(source.begin(), source.end(), source.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  Rules rules = kRules[0];
  for (const Rules& candidate : kRules) {
    if (source == candidate.name) rules = candidate;
  }
  battle_offset_ = rules.offset;
  battle_blocks_ = rules.blocks;
  battle_block_rows_ = rules.block_rows;

  // Rows are addressed by number here, and a rival can write to any of them.
  last_step_row_ = rows_.empty() ? 0 : rows_.back().chart.index;
  std::vector<Row> dense(static_cast<std::size_t>(last_step_row_ + battle_block_rows_ + 2));
  for (std::size_t i = 0; i < dense.size(); ++i) {
    dense[i].chart.index = static_cast<int>(i);
    dense[i].chart.time = chart_.first_row_s + static_cast<double>(i) * chart_.row_seconds;
  }
  for (const Row& row : rows_) {
    if (row.chart.index >= 0) dense[static_cast<std::size_t>(row.chart.index)] = row;
  }
  rows_ = std::move(dense);
}

void Game::update_battle_phase() {
  // Blocks alternate; past the last block the phase simply stays.
  const double seconds = static_cast<double>(tick_) / FixedClock::kHz;
  const int row = static_cast<int>((seconds - chart_.first_row_s) / chart_.row_seconds) + battle_offset_;
  if (row < 0 || row >= battle_blocks_ * battle_block_rows_) return;
  battle_phase_ = row / battle_block_rows_ % 2 == 0 ? 1 : 2;
}

void Game::battle_step(Row& row, int lane, int y) {
  // In the attack phase any panel counts, and what the
  // player pressed becomes this row and, one block later, the rival's row.
  const auto s = static_cast<std::size_t>(lane);
  Pad& pad = pads_[s];
  if (y <= 9 || y >= 0x2b) return;
  if (pad.last_row != row.chart.index && row.judged[s] == Judgment::kNone) pad.latched = false;
  const std::uint16_t mask = row.chart.mask[s];
  const auto target_index = static_cast<std::size_t>(row.chart.index + battle_block_rows_);
  if (target_index >= rows_.size()) return;
  Row& target = rows_[target_index];
  const std::size_t rival = 1 - s;

  if (mask == 0 || pad.latched || pad.active == 0) return;
  pad.latched = true;
  pad.last_row = row.chart.index;
  if (y <= 0x17) return;

  const auto pressed = static_cast<std::uint16_t>(pad.active & 0x1f);
  row.chart.mask[s] = pressed;
  target.chart.mask[rival] = pressed;

  const auto& w = kJudgeBands[static_cast<std::size_t>(options_.margin)];
  Judgment result = Judgment::kNone;
  if (y >= w[0] && y <= w[1]) result = Judgment::kPerfect;
  else if (y >= w[2] && y <= w[3]) result = Judgment::kGreat;
  else if (y >= w[4] && y <= w[5]) result = Judgment::kGood;
  if (result == Judgment::kNone) return;
  target.armed[rival] = true;
  row.judged[s] = result;
  award(lane, result, mask);
}

void Game::apply_modifiers() {
  // The chart changes in this order: copy, Random, then Mirror.
  const auto column = [](std::uint16_t mask, int c) { return (mask >> c) & 1; };
  const std::uint32_t p1 = options_.option_words[0];
  const std::uint32_t p2 = options_.option_words[1];
  const bool both = !options_.double_play && uses_lane_[0] && uses_lane_[1];
  std::mt19937 random(options_.random_seed);

  // Each arrow lands on the left half or the right half.
  const auto shuffle = [&](std::uint16_t five) {
    int notes = 0;
    for (int c = 0; c < 5; ++c) notes += column(five, c);
    std::uint16_t out = 0;
    for (int i = 0; i < notes; ++i) {
      const bool even = random() % 2 == 0;
      const bool right = even ? i == 0 : i != 0;
      const int c = right ? 3 + static_cast<int>(random() % 2) : static_cast<int>(random() % 3);
      out = static_cast<std::uint16_t>(out | (1u << c));
    }
    return out;
  };

  for (Row& row : rows_) {
    auto m = static_cast<std::uint16_t>(options_.double_play ? row.chart.mask[0]
                                                             : row.chart.mask[0] | (row.chart.mask[1] << 5));
    const auto left = static_cast<std::uint16_t>(m & 0x1f);
    if (both && ((p1 | p2) & option_bits::kCopyReversed)) {
      std::uint16_t reversed = 0;
      for (int c = 0; c < 5; ++c) reversed = static_cast<std::uint16_t>(reversed | (column(left, c) << (4 - c)));
      m = static_cast<std::uint16_t>(left | (reversed << 5));
    } else if (both && ((p1 | p2) & option_bits::kCopy)) {
      m = static_cast<std::uint16_t>(left | (left << 5));
    }
    if ((p1 & option_bits::kRandom) && (m & 0x1f)) {
      m = static_cast<std::uint16_t>((m & 0x3e0) | shuffle(m & 0x1f));
    }
    if ((p2 & option_bits::kRandom) && (m & 0x3e0)) {
      m = static_cast<std::uint16_t>((m & 0x1f) | (shuffle(static_cast<std::uint16_t>(m >> 5)) << 5));
    }

    // Mirror only applies when it is the only special option selected.
    const auto swap_columns = [&](int a, int b) {
      const int bit_a = column(m, a);
      const int bit_b = column(m, b);
      if (bit_a != bit_b) m = static_cast<std::uint16_t>(m ^ (1u << a) ^ (1u << b));
    };
    if ((p1 & option_bits::kSpecialMask) == option_bits::kMirror &&
        (p1 & option_bits::kModeMask) != option_bits::kModeDouble) {
      swap_columns(0, 3);
      swap_columns(1, 4);
    }
    if ((p2 & option_bits::kSpecialMask) == option_bits::kMirror) {
      if ((p1 & option_bits::kModeMask) == option_bits::kModeDouble ||
          (p2 & option_bits::kModeMask) == option_bits::kModeDouble) {
        for (int c = 0; c < 5; ++c) swap_columns(c, 9 - c);
      } else {
        swap_columns(5, 8);
        swap_columns(6, 9);
      }
    }

    if (options_.double_play) {
      row.chart.mask = {m, 0};
    } else {
      row.chart.mask = {static_cast<std::uint16_t>(m & 0x1f), static_cast<std::uint16_t>(m >> 5)};
    }
  }
}

void Game::resolve_hud(const Assets& assets) {
  if (const Texture* texture = assets.texture("dec4")) combo_digits_ = *texture;
  if (const Texture* texture = assets.texture("dec2")) score_digits_ = *texture;
  if (!combo_digits_.valid()) missing_.emplace_back("dec4");
  if (!score_digits_.valid()) missing_.emplace_back("dec2");

  const Sprite* sprite = assets.sprite("gamescr.spr");
  if (sprite == nullptr) {
    missing_.emplace_back("gamescr.spr");
    return;
  }
  const std::size_t count = std::min(sprite->quads.size(), static_cast<std::size_t>(kHudSpriteCount));
  for (std::size_t i = 0; i < count; ++i) {
    const SpriteQuad& quad = sprite->quads[i];
    const Texture* texture = assets.texture(quad.texture);
    hud_quads_.push_back({texture != nullptr ? *texture : Texture{}, quad.x, quad.y, quad.w, quad.h,
                          quad.u0, quad.v0, quad.u1, quad.v1});
  }
}

void Game::queue_input(InputEvent event) {
  // Nobody plays the demo.
  if (options_.demo) return;
  if (event.panel >= 0 && event.panel < kPanels) pending_.push_back(event);
}

void Game::apply_input() {
  while (!pending_.empty() && pending_.front().tick <= tick_) {
    const InputEvent event = pending_.front();
    pending_.pop_front();
    if (!event.down) continue;
    // The original reports each press once and ignores how long it is held.
    Pad& pad = pads_[options_.double_play ? 0 : static_cast<std::size_t>(event.panel / 5)];
    const int column = options_.double_play ? event.panel : event.panel % 5;
    pad.pending = static_cast<std::uint16_t>(pad.pending | (1u << column));
  }
}

double Game::row_y(double row_time, double now, std::size_t lane) const {
  return (row_time - now) * pixels_per_second_[lane] + kHitY;
}

int Game::logic_y(double row_time, double now, std::size_t lane) const {
  // Equivalent to row * spacing - trunc(scroll) + 35.
  const double distance = (row_time - now) * pixels_per_second_[lane];
  const bool started = now >= chart_.first_row_s;
  return static_cast<int>(started ? std::ceil(distance - 1e-9) : std::floor(distance + 1e-9)) + static_cast<int>(kHitY);
}

void Game::award(int lane, Judgment result, std::uint16_t mask) {
  // Good resets the combo, a combo above 3 pays 1000 more, four
  // misses in a row cost 1500.
  const auto l = static_cast<std::size_t>(lane);
  PlayerStats& stats = stats_[l];
  switch (result) {
    case Judgment::kPerfect: stats.points += 1000; ++stats.tally[0]; break;
    case Judgment::kGreat: stats.points += 500; ++stats.tally[1]; break;
    case Judgment::kGood: stats.points += 100; ++stats.tally[2]; break;
    case Judgment::kPressMiss: stats.points -= 700; ++stats.tally[3]; break;
    case Judgment::kLateMiss: stats.points -= 1000; ++stats.tally[4]; break;
    case Judgment::kNone: return;
  }
  if (result == Judgment::kPerfect || result == Judgment::kGreat) {
    ++stats.combo;
    stats.miss_run = 0;
    for (std::size_t column = 0; column < kPanels; ++column) {
      if ((mask >> column) & 1) receptors_[l][column] = {ReceptorState::kHit, 0};
    }
  } else if (result == Judgment::kGood) {
    stats.combo = 0;
    stats.miss_run = 0;
  } else {
    stats.combo = 0;
    ++stats.miss_run;
  }
  stats.max_combo = std::max(stats.max_combo, stats.combo);
  if (stats.combo > 3) stats.points += 1000;
  if (stats.miss_run > 3) stats.points -= 1500;
  stats.points = std::max(0, stats.points);
  stats.last = result;
  judgment_[l] = {true, result, 0, 0};
  update_life(lane, result);
}

void Game::update_life(int lane, Judgment result) {
  // Danger and failure only apply in normal play.
  const LifeRow& row = kLifeTable[static_cast<std::size_t>(options_.life_level)];
  PlayerStats& stats = stats_[static_cast<std::size_t>(lane)];
  switch (result) {
    case Judgment::kPerfect: stats.life += row.perfect; break;
    case Judgment::kGreat: stats.life += row.great; break;
    case Judgment::kGood: stats.life += row.good; break;
    case Judgment::kPressMiss: stats.life += row.bad; break;
    case Judgment::kLateMiss: stats.life += row.miss; ++stats.late_misses; break;
    case Judgment::kNone: return;
  }
  if (stats.combo > 3) stats.life += row.combo;
  if (stats.miss_run > 3) stats.life += static_cast<float>(stats.miss_run) * row.run;

  const double seconds = static_cast<double>(tick_) / FixedClock::kHz;
  const int current_row = static_cast<int>((seconds - chart_.first_row_s) / chart_.row_seconds);
  // STAGE BREAK: the first stages cannot be failed by losing all the life.
  // Battle and Non-Stop never enter danger, so they cannot be failed either.
  const bool can_fail =
      !options_.demo && !battle_ && options_.flight == nullptr && options_.stage >= options_.stage_break + 1;
  if (stats.life < 3.0f && !stats.danger && can_fail && grace_rows_ < current_row) {
    stats.danger = true;
    danger_frames_ = 0;
  }
  if (stats.life >= 3.0f && stats.danger) {
    stats.danger = false;
    danger_frames_ = 0;
  }
  if (stats.life <= row.minimum) {
    stats.life = row.minimum;
    const bool settled = stats.state == LifeState::kAbsent || stats.state == LifeState::kBelowLine ||
                         stats.state == LifeState::kCleared;
    if (!settled && can_fail && danger_frames_ > 600) stats.state = LifeState::kFailed;
  }
  if (stats.life > 42.0f) stats.life = 42.0f;
}

LifeState Game::lane_state(std::size_t lane) const { return stats_[lane].state; }

void Game::update_end_states() {
  // Decide how each player finished once the chart is over.
  if (rows_.empty()) return;
  const double seconds = static_cast<double>(tick_) / FixedClock::kHz;
  const int current_row = static_cast<int>((seconds - chart_.first_row_s) / chart_.row_seconds);
  if (battle_) {
    // Battle cannot be lost: both finish together once the last sent block is over.
    if (last_step_row_ + battle_block_rows_ + 10 <= current_row) {
      for (PlayerStats& stats : stats_) stats.state = LifeState::kCleared;
    }
    return;
  }
  if (rows_.back().chart.index + 5 <= current_row) {
    for (std::size_t lane = 0; lane < 2; ++lane) {
      PlayerStats& stats = stats_[lane];
      if (stats.state == LifeState::kFailed || stats.state == LifeState::kAbsent) continue;
      stats.state = stats.life <= 1.0f || lane_rows_[lane] / 2 < stats.late_misses ? LifeState::kBelowLine
                                                                                   : LifeState::kCleared;
    }
  }

  const bool danger = uses_lane_[0] && uses_lane_[1] ? stats_[0].danger && stats_[1].danger
                      : uses_lane_[0]               ? stats_[0].danger
                                                    : stats_[1].danger;
  if (danger) ++danger_frames_;
}

void Game::update_ending() {
  // The result animation is drawn from gameview.bga. The counter
  // only runs the ending; the clip time is a separate value that is held or
  // jumps, so "stage clear" and "stage failed" are different parts of the clip.
  const auto is = [&](std::size_t lane, std::initializer_list<LifeState> states) {
    return std::find(states.begin(), states.end(), lane_state(lane)) != states.end();
  };

  if (is(0, {LifeState::kFailed, LifeState::kAbsent}) && is(1, {LifeState::kFailed, LifeState::kAbsent})) {
    ending_ = Ending::kFail;
    ++ending_counter_;
    if (ending_phase_ == 0) {
      if (ending_counter_ < 0x5a) {
        ending_time_ = ending_counter_;
      } else {
        hide_playfield_ = true;
        ending_time_ = 0x5a;
      }
      if (ending_counter_ == 0x16) {
        audio_stop_ = true;
        sounds_.play(sfx::kFailEnding, 7);
      } else if (ending_counter_ > 0xef) {
        ending_phase_ = 1;
        ending_counter_ = 0;
        ending_time_ = 0;
      }
    } else {
      ending_time_ = std::min(ending_counter_, 0xf0);
      if (ending_counter_ > 0x77) ending_brightness_ = std::max(0.0f, ending_brightness_ - 0.02f);
      if (ending_counter_ > 0xef) {
        ending_counter_ = 0xf0;
        ended_ = true;
      }
    }
    return;
  }
  if (is(0, {LifeState::kFailed, LifeState::kBelowLine, LifeState::kAbsent}) &&
      is(1, {LifeState::kFailed, LifeState::kBelowLine, LifeState::kAbsent})) {
    ending_ = Ending::kBelowLine;
    ++ending_counter_;
    ending_time_ = ending_counter_ < 0x1f ? ending_counter_ : ending_counter_ + 0x70;
    if (ending_time_ > 0xd8) blackout_ = true;
    if (ending_time_ > 0x167) ending_brightness_ = std::max(0.0f, ending_brightness_ - 0.02f);
    if (ending_counter_ == 0xf) audio_stop_ = true;
    if (ending_counter_ >= 0x1e0) {
      ending_counter_ = 0x1e0;
      ended_ = true;
    }
    return;
  }
  if (lane_state(0) == LifeState::kCleared || lane_state(1) == LifeState::kCleared) {
    ending_ = Ending::kClear;
    ++ending_counter_;
    ending_time_ = std::min(ending_counter_, battle_ ? 0x1e : 0x78);
    if (ending_counter_ >= (battle_ ? 0x3c : 0xf0)) {
      ending_counter_ = 0xf0;
      ended_ = true;
    }
  }
}

bool Game::take_audio_stop() {
  const bool stop = audio_stop_;
  audio_stop_ = false;
  return stop;
}

void Game::judge(Row& row, int lane, int y) {
  const auto& w = kJudgeBands[static_cast<std::size_t>(options_.margin)];
  Judgment result = Judgment::kPressMiss;
  if (y >= w[0] && y <= w[1]) result = Judgment::kPerfect;
  else if (y >= w[2] && y <= w[3]) result = Judgment::kGreat;
  else if (y >= w[4] && y <= w[5]) result = Judgment::kGood;

  const auto s = static_cast<std::size_t>(lane);
  // The original can judge a Good or Bad row again; its results screen counts the final result.
  if (row.judged[s] == Judgment::kGood) --stats_[s].tally[2];
  if (row.judged[s] == Judgment::kPressMiss) --stats_[s].tally[3];
  row.judged[s] = result;
  award(lane, result, row.chart.mask[s]);
}

void Game::autoplay_rows(double now) {
  // Every row is hit the moment it reaches the
  // Perfect band; 69 of 100 count as Perfect and the rest as Good.
  const auto& w = kJudgeBands[static_cast<std::size_t>(options_.margin)];
  for (Row& row : rows_) {
    for (int lane = 0; lane < 2; ++lane) {
      const auto s = static_cast<std::size_t>(lane);
      const std::uint16_t mask = row.chart.mask[s];
      const int y = logic_y(row.chart.time, now, s);
      if (mask == 0 || y < w[0] || y > w[1] || row.judged[s] != Judgment::kNone) continue;
      const Judgment result = random_() % 100 > kAutoplayGoodPercent ? Judgment::kPerfect : Judgment::kGood;
      row.judged[s] = result;
      award(lane, result, mask);
    }
  }
}

void Game::update_rows(double now) {
  if (options_.demo) {
    autoplay_rows(now);
    return;
  }
  if (battle_) update_battle_phase();
  for (Row& row : rows_) {
    for (int lane = 0; lane < 2; ++lane) {
      const auto s = static_cast<std::size_t>(lane);
      const int y = logic_y(row.chart.time, now, s);
      if (y <= -64 || y >= 480) continue;
      if (battle_ && battle_phase_ != 2) {
        if (battle_phase_ == 1) battle_step(row, lane, y);
        continue;
      }
      const std::uint16_t mask = row.chart.mask[s];
      const Judgment previous = row.judged[s];
      // A Perfect or Great row stops being drawn, so the game no longer sees it.
      if (mask == 0 || previous == Judgment::kPerfect || previous == Judgment::kGreat) continue;
      Pad& pad = pads_[s];
      if (y < kZoneLate) {
        if (previous == Judgment::kNone && (!battle_ || row.armed[s])) {
          row.judged[s] = Judgment::kLateMiss;
          award(lane, Judgment::kLateMiss, mask);
        }
      } else if (y <= kZoneEarly) {
        if (pad.last_row != row.chart.index && previous == Judgment::kNone) pad.latched = false;
        if (!pad.latched && (mask & ~pad.active) == 0) {
          pad.latched = true;
          pad.last_row = row.chart.index;
          judge(row, lane, y);
        }
      }
    }
  }
}

void Game::run_frame() {
  // The press masks are cleared every fifth poll.
  if (++poll_count_ > kMaskClearPolls) {
    poll_count_ = 0;
    for (Pad& pad : pads_) pad.active = 0;
  }
  for (std::size_t lane = 0; lane < 2; ++lane) {
    Pad& pad = pads_[lane];
    for (std::size_t column = 0; column < kPanels; ++column) {
      if (((pad.pending >> column) & 1) && receptors_[lane][column].state != ReceptorState::kHit) {
        receptors_[lane][column] = {ReceptorState::kPressed, 0};
      }
    }
    pad.active = static_cast<std::uint16_t>(pad.active | pad.pending);
    pad.pending = 0;
  }

  // The original draws receptors before judging, and advances them as it draws.
  drawn_receptors_ = receptors_;
  for (ReceptorSet& lane : receptors_) {
    for (ReceptorFx& fx : lane) {
      if (fx.state != ReceptorState::kIdle && ++fx.timer >= kReceptorFxFrames) fx = {};
    }
  }

  update_rows(static_cast<double>(tick_) / FixedClock::kHz);

  drawn_judgment_ = judgment_;
  for (std::size_t lane = 0; lane < 2; ++lane) {
    drawn_judgment_[lane].combo = stats_[lane].combo;
    JudgmentFx& text = judgment_[lane];
    if (text.active && ++text.timer > kJudgmentFrames) text = {};
  }

  // The camera advances one key every second frame.
  if (options_.flight != nullptr && !hide_playfield_) {
    if (++flight_counter_ > static_cast<int>(options_.flight->camera.size()) * 2) flight_counter_ = 0;
  }

  ++flicker_frame_;
  if (!options_.demo) {
    update_ending();
    update_end_states();
  }
}

void Game::step() {
  apply_input();

  // Receptors light up for 24 steps per beat.
  const double seconds = static_cast<double>(tick_) / FixedClock::kHz;
  const int beat = static_cast<int>((seconds - chart_.first_row_s) / (chart_.row_seconds * 2.0));
  if (beat != beat_number_) {
    beat_number_ = beat;
    beat_flash_ = kBeatFlashTicks;
  } else if (beat_flash_ > 0) {
    --beat_flash_;
  }

  if (tick_ % kStepsPerFrame == 0) run_frame();
  ++tick_;
}

bool Game::finished() const {
  if (!options_.demo) return ended_;
  // The demo ends after 51 seconds of song, or with the first coin.
  return static_cast<double>(tick_) / FixedClock::kHz >= kDemoSeconds ||
         (options_.coins != nullptr && options_.coins->coins() != 0);
}

void Game::draw_bga(DrawList& out, double seconds) const {
  if (options_.flight != nullptr) {
    const int last = static_cast<int>(options_.flight->camera.size()) - 1;
    const int frame = std::clamp(flight_counter_ / 2, 0, last);
    out.set_scene(options_.flight, scene_matrix(options_.flight->camera[static_cast<std::size_t>(frame)]),
                  scene_base_model(frame));
    return;
  }
  const double t = seconds * kBgaTicksPerSecond - bga_.strt * 0.6;
  layers_.draw(out, static_cast<float>(std::max(0.0, t)), 1.0f);
}

void Game::draw_arrow_tile(DrawList& out, int sheet, int frame, float x, float y, float w, float h, int turn,
                           float alpha) const {
  const Texture& texture = arrow_textures_[static_cast<std::size_t>(sheet)];
  if (!texture.valid()) return;
  const auto u0 = static_cast<float>(kCell * (frame % kGrid));
  const auto v0 = static_cast<float>(kCell * (frame / kGrid));
  emit_quad(out, texture, x, y, w, h, u0, v0, u0 + kArrowSize, v0 + kArrowSize, kIdentity,
            {1.0f, 1.0f, 1.0f, alpha}, turn);
}

float Game::column_x(int lane, int column) const {
  if (options_.double_play) return kDoubleColumnX[static_cast<std::size_t>(column)];
  return kColumnX[static_cast<std::size_t>(lane)][static_cast<std::size_t>(column)];
}

void Game::draw_receptors(DrawList& out) const {
  const int beat_frame = beat_flash_ > 0 ? 1 : 0;
  for (int lane = 0; lane < 2; ++lane) {
    const auto l = static_cast<std::size_t>(lane);
    if (!uses_lane_[l]) continue;
    const int pads = options_.double_play ? 2 : 1;

    for (int pad = 0; pad < pads; ++pad) {
      for (int ordered : kReceptorDrawOrder) {
        const int column = ordered + pad * 5;
        const auto art = static_cast<std::size_t>(ordered);
        const ReceptorFx& fx = drawn_receptors_[l][static_cast<std::size_t>(column)];
        const auto t = static_cast<std::size_t>(std::min(fx.timer, kReceptorFxFrames - 1));
        const float scale = fx.state == ReceptorState::kHit       ? kHitScale[t]
                            : fx.state == ReceptorState::kPressed ? kPressScale[t]
                                                                  : 1.0f;
        const float half = std::floor(kReceptorBaseSize / 2.0f);
        const float x = column_x(lane, column);
        const auto size = std::round(kReceptorBaseSize * scale);
        draw_arrow_tile(out, kReceptorSheet[art], kReceptorBaseFrame[art] + beat_frame,
                        std::round(x + half - half * scale), std::round(kReceptorY + half - half * scale), size,
                        size, kTurn[art]);
      }
    }

    for (int column = 0; column < columns(); ++column) {
      const auto c = static_cast<std::size_t>(column);
      const ReceptorFx& fx = drawn_receptors_[l][c];
      if (fx.state == ReceptorState::kIdle) continue;
      const auto t = static_cast<std::size_t>(std::min(fx.timer, kReceptorFxFrames - 1));
      const auto art = static_cast<std::size_t>(column % 5);
      const bool center = art == 2;
      const bool hit = fx.state == ReceptorState::kHit;
      const int sheet = hit ? (center ? kHitFxSheetCenter[t] : kHitFxSheet[t])
                            : (center ? kPressFxSheetCenter[t] : kPressFxSheet[t]);
      const int frame = hit ? (center ? kHitFxFrameCenter[t] : kHitFxFrame[t])
                            : (center ? kPressFxFrameCenter[t] : kPressFxFrame[t]);
      draw_arrow_tile(out, sheet, frame, column_x(lane, column), kReceptorY, kArrowSize, kArrowSize, kTurn[art]);
    }
  }
}

void Game::draw_arrows(DrawList& out, double seconds) const {
  const double beat = (seconds - chart_.first_row_s) / chart_.row_seconds / 2.0;
  // The six-per-beat counter is offset by three.
  const int six = static_cast<int>(std::floor(beat * 6.0));
  const auto phase = static_cast<std::size_t>((((six + 3) % 6) + 6) % 6);
  const float size = kArrowSize;
  // Battle's attack rows hold one frame of the animation, which shows them white.
  const bool attacking = battle_ && battle_phase_ == 1;
  const std::size_t drawn_phase = attacking ? 3 : phase;
  if (battle_ && battle_phase_ == 0) return;
  for (const Row& row : rows_) {
    for (int lane = 0; lane < 2; ++lane) {
      const auto l = static_cast<std::size_t>(lane);
      const double y = row_y(row.chart.time, seconds, l);
      if (y <= -64.0 || y >= 480.0) continue;
      if (!attacking && (row.judged[l] == Judgment::kPerfect || row.judged[l] == Judgment::kGreat)) continue;
      // Vanish fades the arrows out above the middle of the screen.
      float alpha = 1.0f;
      if (!battle_ && (options_.option_words[l] & option_bits::kVanish) && y < kVanishStartY) {
        alpha = std::max(0.0f, 1.0f - static_cast<float>(kVanishStartY - y) * kVanishFade);
      }
      for (int column = 0; column < columns(); ++column) {
        if ((row.chart.mask[l] >> column) & 1) {
          const auto art = static_cast<std::size_t>(column % 5);
          draw_arrow_tile(out, kSheet[art][drawn_phase], kFrame[art][drawn_phase], column_x(lane, column),
                          static_cast<float>(y), size, size, kTurn[art], alpha);
        }
      }
    }
  }
}

const Game::ResolvedQuad* Game::hud_sprite(int id) const {
  const auto index = static_cast<std::size_t>(id);
  return index < hud_quads_.size() && hud_quads_[index].texture.valid() ? &hud_quads_[index] : nullptr;
}

void Game::draw_sprite(DrawList& out, int id, float scale, float offset_y, Color color) const {
  const ResolvedQuad* q = hud_sprite(id);
  if (q == nullptr) return;
  // Scales around the center, using integer half sizes.
  const float half_w = std::floor(q->w / 2.0f);
  const float half_h = std::floor(q->h / 2.0f);
  emit_quad(out, q->texture, std::round(q->x + half_w - half_w * scale),
            std::round(q->y + half_h - half_h * scale) + offset_y, std::round(q->w * scale),
            std::round(q->h * scale), q->u0, q->v0, q->u1, q->v1, kIdentity, color, 0);
}

void Game::draw_judgment(DrawList& out, int lane) const {
  const auto l = static_cast<std::size_t>(lane);
  const JudgmentFx& text = drawn_judgment_[l];
  if (!text.active) return;

  const int base = options_.double_play ? kTextSpriteDouble : lane == 0 ? kTextSpritePlayer1 : kTextSpritePlayer2;
  const auto t = static_cast<std::size_t>(std::min(text.timer, 19));
  int id = base;
  float scale = kTextGrowScale[t];
  switch (text.result) {
    case Judgment::kGreat: id = base - 1; break;
    case Judgment::kGood: id = base - 2; break;
    case Judgment::kPressMiss: id = base - 3; scale = kTextBounceScale[t]; break;
    case Judgment::kLateMiss: id = base - 4; scale = kTextBounceScale[t]; break;
    default: break;
  }
  draw_sprite(out, id, scale, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f});

  if (text.combo < 4 || !combo_digits_.valid()) return;
  const float alpha = std::min(1.0f, static_cast<float>(t) * 0.1f);
  const Color color = {1.0f, 1.0f, 1.0f, alpha};
  const float bounce = kTextBounceScale[t];
  const int label = options_.double_play ? kComboLabelDouble : lane == 0 ? kComboLabelPlayer1 : kComboLabelPlayer2;
  draw_sprite(out, label, 1.0f, std::round(64.0f - bounce * 64.0f), color);

  float x = options_.double_play ? kComboDigitXDouble : lane == 0 ? kComboDigitX1 : kComboDigitX2;
  const std::string digits = zero_padded(text.combo, 3);
  const float y = std::round(318.0f - bounce * 64.0f);
  for (char digit : digits) {
    const int frame = digit - '0';
    const auto u0 = static_cast<float>(64 * (frame % 4));
    const auto v0 = static_cast<float>(64 * (frame / 4));
    emit_quad(out, combo_digits_, x, y, 64.0f, 64.0f, u0, v0, u0 + 64.0f, v0 + 64.0f, kIdentity, color, 0);
    x += kComboDigitStep;
  }
}

void Game::draw_score(DrawList& out, int lane) const {
  const Color white = {1.0f, 1.0f, 1.0f, 1.0f};
  draw_sprite(out, lane == 0 ? kScoreLabelPlayer1 : kScoreLabelPlayer2, 1.0f, 0.0f, white);
  if (!score_digits_.valid()) return;
  float x = lane == 0 ? kScoreDigitX1 : kScoreDigitX2;
  for (char digit : zero_padded(stats_[static_cast<std::size_t>(lane)].points, 7)) {
    const int frame = digit - '0';
    const auto u0 = static_cast<float>(32 * (frame % 8));
    const auto v0 = static_cast<float>(39 * (frame / 8));
    emit_quad(out, score_digits_, x, kScoreDigitY, 32.0f, 35.0f, u0, v0, u0 + 32.0f, v0 + 35.0f, kIdentity, white, 0);
    x += kScoreDigitStep;
  }
}

void Game::draw_life_bar(DrawList& out, int lane) const {
  // The fill grows from the center and is drawn
  // only on every other frame drawn: the toggle lives in the drawing code, not
  // in the game clock. With two players the bars take turns: footage of the real
  // machine never shows both lit, nor both dark, in the same frame.
  const auto l = static_cast<std::size_t>(lane);
  const Color white = {1.0f, 1.0f, 1.0f, 1.0f};
  draw_sprite(out, kLifeBaseSprite[l], 1.0f, 0.0f, white);
  if (((drawn_frames_ + lane) & 1) != 0) return;
  const ResolvedQuad* q = hud_sprite(kLifeFillSprite[l]);
  if (q == nullptr) return;

  const float life = stats_[l].life;
  const auto width = std::max(6.0f, life >= 0.0f ? static_cast<float>(std::nearbyint(life)) * 6.0f : 0.0f) + 2.0f;
  const auto texture_w = static_cast<float>(q->texture.width);
  if (lane == 0) {
    emit_quad(out, q->texture, q->x + texture_w - width, q->y, width, q->h, q->u0 + texture_w - width, q->v0,
              q->u0 + texture_w, q->v1, kIdentity, white, 0);
  } else {
    emit_quad(out, q->texture, q->x, q->y, width, q->h, q->u0, q->v0, q->u1 + texture_w - width, q->v1, kIdentity,
              white, 0);
  }
}

void Game::draw_sprite_at(DrawList& out, int id, float x, float y) const {
  const ResolvedQuad* q = hud_sprite(id);
  if (q == nullptr) return;
  emit_quad(out, q->texture, x, y, q->w, q->h, q->u0, q->v0, q->u1, q->v1, kIdentity, {1.0f, 1.0f, 1.0f, 1.0f}, 0);
}

void Game::draw_icons(DrawList& out, int lane) const {
  // Modifiers stack down the screen, next to the player's side.
  const auto l = static_cast<std::size_t>(lane);
  const std::uint32_t word = options_.option_words[l];
  const float x = kIconX[l];
  float y = kIconStartY;
  if (word & option_bits::kBattle) {
    y = 0x96;
    draw_sprite_at(out, 0x3d, x, y);
  }
  const auto next = [&](int id) {
    y += kIconStepY;
    draw_sprite_at(out, id, x, y);
  };
  if (word & option_bits::kSpeedMask) next(0x3f);
  if ((word & option_bits::kModeMask) == option_bits::kModeDouble) next(0x3e);
  if (word & option_bits::kMirror) next(0x41);
  if (word & option_bits::kVanish) next(0x40);
  if (word & option_bits::kCopy) {
    draw_sprite_at(out, 0x42, x, y + kIconStepY);
  } else if (word & option_bits::kCopyReversed) {
    draw_sprite_at(out, 0x3c, x, y + kIconStepY);
  }
}

void Game::draw_credits(DrawList& out) const {
  if (options_.coins == nullptr) return;
  const bool p1 = uses_lane_[0] && !options_.demo;
  const bool p2 = uses_lane_[1] && !options_.double_play && !options_.demo;
  credit_hud_.draw(out, *options_.coins, options_.demo ? CreditHud::Screen::kDemo : CreditHud::Screen::kPlay, p1, p2,
                   options_.double_play, flicker_frame_);
}

void Game::draw_stage(DrawList& out) const {
  // Battle and Non-Stop are shown as the final stage.
  const int sprite = options_.stage == 1 && (battle_ || options_.flight != nullptr)
                         ? kStageSprite[3]
                         : kStageSprite[static_cast<std::size_t>(options_.stage)];
  draw_sprite(out, sprite, 1.0f, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f});
}

void Game::draw(DrawList& out, double alpha) const {
  const double seconds = (static_cast<double>(tick_) + alpha) / FixedClock::kHz;
  if (!blackout_) {
    // Once the fail clip hides the playfield, the song's BGA goes too.
    if (!hide_playfield_) draw_bga(out, seconds);
    const bool danger = uses_lane_[0] && uses_lane_[1] ? stats_[0].danger && stats_[1].danger
                        : uses_lane_[0]               ? stats_[0].danger
                                                      : stats_[1].danger;
    if (danger && !hide_playfield_) {
      gameview_layers_.draw(out, static_cast<float>(danger_frames_ % 300 + 0x348), 1.0f);
    }
    if (!hide_playfield_) {
      draw_receptors(out);
      draw_arrows(out, seconds);
      for (int lane = 0; lane < 2; ++lane) {
        if (uses_lane_[static_cast<std::size_t>(lane)]) draw_judgment(out, lane);
      }
      if (!options_.demo) {
        for (int lane = 0; lane < 2; ++lane) {
          if (uses_lane_[static_cast<std::size_t>(lane)]) draw_score(out, lane);
        }
      }
      for (int lane = 0; lane < 2; ++lane) {
        if (!uses_lane_[static_cast<std::size_t>(lane)]) continue;
        draw_life_bar(out, lane);
        draw_icons(out, lane);
      }
      if (!options_.demo) draw_stage(out);
    }
  }

  // The ending goes over the playfield and under the credits.
  if (ending_ == Ending::kFail) {
    const float t = static_cast<float>(ending_time_ + (ending_phase_ == 0 ? 600 : 240));
    gameview_layers_.draw(out, t, ending_phase_ == 0 ? 1.0f : ending_brightness_);
  } else if (ending_ == Ending::kBelowLine) {
    gameview_layers_.draw(out, static_cast<float>(ending_time_), ending_brightness_);
  } else if (ending_ == Ending::kClear) {
    gameview_layers_.draw(out, static_cast<float>(ending_time_), 1.0f);
  }

  if (!hide_playfield_) draw_credits(out);
  if (options_.demo) draw_demo_label(out);
  ++drawn_frames_;
}

void Game::draw_demo_label(DrawList& out) const {
  // The two DEMO PLAY sprites alternate on each frame drawn. In footage
  // of the real machine the bright frame is the one where the left life bar is lit.
  const Color color = drawn_frames_ % 2 == 0 ? Color{1.0f, 1.0f, 0.3f, 1.0f} : Color{0.3f, 0.3f, 0.0f, 0.3f};
  draw_sprite(out, 0, 1.0f, 0.0f, color);
  draw_sprite(out, 1, 1.0f, 0.0f, color);
}

void Game::draw_overlay(DrawList& out, float time, float brightness) const {
  gameview_layers_.draw(out, time, brightness);
}

char Game::grade(int player) const {
  // Thresholds, which are one point higher for player 2.
  const auto p = static_cast<std::size_t>(player);
  const auto& t = stats_[p].tally;
  const int rows = std::max(1, lane_rows_[p]);
  const int percent = (t[0] * 2 + t[1] - t[3] - t[4] * 2) * 100 / (rows * 2);
  const int extra = player == 1 ? 1 : 0;
  if (percent >= 85 + extra && t[3] == 0 && t[4] == 0) return 'S';
  if (percent >= 80 + extra) return 'A';
  if (percent >= 70 + extra) return 'B';
  if (percent >= 50 + extra) return 'C';
  if (percent >= 40 + extra) return 'D';
  return stats_[p].state == LifeState::kCleared ? 'D' : 'F';
}

}  // namespace piu
