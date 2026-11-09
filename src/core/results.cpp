// Counting animation, grade reveal, closing fade and drawing of the results screen.

#include "core/results.h"

#include <algorithm>
#include <string>

namespace piu {
namespace {

constexpr int kSpriteCount = 0xdd;
constexpr int kStepsPerFrame = 4;
// The frame counter runs at 60 Hz: the interrupt runs at 240 Hz and adds one every fourth call.
constexpr int kRowDelayFrames = 20;
constexpr int kHoldFrames = 60;
constexpr int kFinalFrames = 180;
constexpr int kLetterSoundFrame = 61;
constexpr float kRowFadePerFrame = 0.05f;
constexpr float kCloseFadePerFrame = 0.02f;
constexpr int kCloseFirstOverlayTime = 0xf0;
constexpr int kCloseLastOverlayTime = 0x1e0;

constexpr std::array<int, 6> kRowY = {0x61, 0x91, 0xc4, 0xf4, 0x124, 0x154};
constexpr std::array<int, 5> kRowLabel = {0xb4, 0xb3, 0xb2, 0xb1, 0xb0};
constexpr std::array<int, 2> kMaxComboLabels = {0xae, 0xaf};
constexpr std::array<int, 2> kDigitX = {0x49, 0x1ea};
constexpr std::array<float, 2> kScoreX = {0x18, 0x1b0};
constexpr std::array<int, 2> kScoreLabel = {0x3b, 0x3a};

constexpr int kBattleRightLoses = 0x48;
constexpr int kBattleRightWins = 0x49;
constexpr int kBattleLeftLoses = 0x4a;
constexpr int kBattleLeftWins = 0x4b;

struct LetterRange {
  int first, count;
};

// In the order S, A, B, C, D, F.
constexpr std::array<std::array<LetterRange, 6>, 2> kLetters = {{
    {{{0xc7, 0xb}, {0x4c, 0xa}, {0x56, 0x9}, {0x5f, 0xb}, {0x6a, 0xa}, {0x74, 0x8}}},
    {{{0xd2, 0xb}, {0x7c, 0xa}, {0x86, 0x9}, {0x8f, 0xb}, {0x9a, 0xa}, {0xa4, 0x8}}},
}};

std::size_t letter_index(char grade) {
  switch (grade) {
    case 'S': return 0;
    case 'A': return 1;
    case 'B': return 2;
    case 'C': return 3;
    case 'D': return 4;
    default: return 5;
  }
}

std::string zero_padded(int value, std::size_t width) {
  std::string text = std::to_string(value);
  if (text.size() < width) text.insert(0, width - text.size(), '0');
  return text;
}

void emit(DrawList& out, const Texture& texture, float x, float y, float w, float h, float u0, float v0, float u1,
          float v1, Color color) {
  const auto tw = static_cast<float>(texture.width);
  const auto th = static_cast<float>(texture.height);
  inset_texels(u0, u1);
  inset_texels(v0, v1);
  out.quad(texture.id, {x, y, u0 / tw, v0 / th, color}, {x + w, y, u1 / tw, v0 / th, color},
           {x + w, y + h, u1 / tw, v1 / th, color}, {x, y + h, u0 / tw, v1 / th, color});
}

}  // namespace

ResultsScreen::ResultsScreen(const std::array<ResultPlayer, 2>& players, const Assets& assets, bool game_over,
                             bool battle)
    : players_(players), game_over_(game_over), battle_(battle), overlay_time_(kCloseFirstOverlayTime) {
  if (const Texture* texture = assets.texture("dec2")) digits_ = *texture;
  if (const Sprite* sprite = assets.sprite("gamescr.spr")) {
    const std::size_t count = std::min(sprite->quads.size(), static_cast<std::size_t>(kSpriteCount));
    for (std::size_t i = 0; i < count; ++i) {
      const SpriteQuad& quad = sprite->quads[i];
      const Texture* texture = assets.texture(quad.texture);
      quads_.push_back({texture != nullptr ? *texture : Texture{}, quad.x, quad.y, quad.w, quad.h, quad.u0, quad.v0,
                        quad.u1, quad.v1});
    }
  }
}

void ResultsScreen::press_center(int player) {
  if (phase_ == Phase::kCounting && (player == 0 || player == 1) && players_[static_cast<std::size_t>(player)].active) {
    skip_ = true;
  }
}

void ResultsScreen::step() {
  if (ticks_++ % kStepsPerFrame == 0) run_frame();
}

void ResultsScreen::run_frame() {
  ++phase_frames_;
  if (phase_ == Phase::kCounting) {
    bool previous_done = true;
    bool all_done = true;
    for (std::size_t row = 0; row < started_.size(); ++row) {
      if (!previous_done) {
        all_done = false;
        break;
      }
      if (!started_[row]) {
        started_[row] = true;
        start_time_[row] = phase_frames_;
        sounds_.play(sfx::kStart, 1);
      }
      bool reached = true;
      for (std::size_t p = 0; p < 2; ++p) {
        if (!players_[p].active) continue;
        shown_[row][p] = std::min(shown_[row][p] + 1, players_[p].counts[row]);
        reached = reached && shown_[row][p] == players_[p].counts[row];
      }
      previous_done = reached && phase_frames_ - start_time_[row] > kRowDelayFrames;
      all_done = all_done && previous_done;
    }
    if (all_done || skip_) {
      phase_ = Phase::kHold;
      phase_frames_ = 0;
    }
  } else if (phase_ == Phase::kHold) {
    if (phase_frames_ >= kHoldFrames) {
      phase_ = Phase::kFinal;
      phase_frames_ = 0;
      sounds_.play(sfx::kGrade, 2);
    }
  } else if (phase_ == Phase::kFinal && phase_frames_ <= kFinalFrames) {
    // This runs on the frame after the 1 s timer sets its flag.
    if (phase_frames_ > kLetterSoundFrame && !letter_played_) {
      letter_played_ = true;
      play_letter();
    }
  } else if (phase_ == Phase::kFinal) {
    phase_ = game_over_ ? Phase::kClosing : Phase::kDone;
    phase_frames_ = 0;
  } else if (phase_ == Phase::kClosing) {
    // The screen dims while the closing clip plays over it.
    screen_brightness_ = std::max(0.0f, screen_brightness_ - kCloseFadePerFrame);
    overlay_time_ = std::min(overlay_time_ + 1, kCloseLastOverlayTime);
    if (phase_frames_ > kFinalFrames) {
      overlay_brightness_ = fade_;
      fade_ -= kCloseFadePerFrame;
      if (fade_ <= 0.0f) phase_ = Phase::kDone;
    }
  }
}

void ResultsScreen::play_letter() {
  // Battle takes another branch, which plays nothing.
  if (battle_) return;
  // The best letter on screen speaks; S has no voice of its own.
  std::size_t best = 5;
  for (const ResultPlayer& player : players_) {
    if (player.active) best = std::min(best, letter_index(player.grade));
  }
  if (best == 0) return;
  const int offset = static_cast<int>(best) - 1;
  sounds_.play(sfx::kLetterA + offset, 3);
  sounds_.play(sfx::kVoiceA + offset, 6);
}

void ResultsScreen::draw_sprite(DrawList& out, int id, Color color) const {
  const auto index = static_cast<std::size_t>(id);
  if (index >= quads_.size() || !quads_[index].texture.valid()) return;
  const Quad& q = quads_[index];
  emit(out, q.texture, q.x, q.y, q.w, q.h, q.u0, q.v0, q.u1, q.v1, color);
}

void ResultsScreen::draw_row(DrawList& out, int row, Color color, bool final_values) const {
  const auto r = static_cast<std::size_t>(row);
  if (row < 5) {
    draw_sprite(out, kRowLabel[r], color);
  } else {
    for (int id : kMaxComboLabels) draw_sprite(out, id, color);
  }
  if (!digits_.valid()) return;

  for (std::size_t p = 0; p < 2; ++p) {
    if (!players_[p].active) continue;
    const int value = final_values ? players_[p].counts[r] : shown_[r][p];
    auto x = static_cast<float>(kDigitX[p]);
    for (char digit : zero_padded(value, 3)) {
      const int frame = digit - 0x26;
      const auto u0 = static_cast<float>(32 * (frame % 8));
      const auto v0 = static_cast<float>(39 * (frame / 8));
      emit(out, digits_, x, static_cast<float>(kRowY[r]), 32.0f, 39.0f, u0, v0, u0 + 31.0f, v0 + 38.0f, color);
      x += 25.0f;
    }
  }
}

void ResultsScreen::draw(DrawList& out) const {
  // After the closing fade the screen stays dark until the platform moves on.
  const float b = phase_ == Phase::kClosing || (phase_ == Phase::kDone && game_over_) ? screen_brightness_ : 1.0f;
  const Color white = {b, b, b, 1.0f};
  for (int id = 0xb7; id < 0xbf; ++id) draw_sprite(out, id, white);
  draw_sprite(out, 0xb5, white);
  draw_sprite(out, 0xb6, white);

  for (std::size_t p = 0; p < 2; ++p) {
    if (!players_[p].active) continue;
    draw_sprite(out, kScoreLabel[p], white);
    if (!digits_.valid()) continue;
    float x = kScoreX[p];
    for (char digit : zero_padded(players_[p].points, 7)) {
      const int frame = digit - '0';
      const auto u0 = static_cast<float>(32 * (frame % 8));
      const auto v0 = static_cast<float>(39 * (frame / 8));
      emit(out, digits_, x, 420.0f, 32.0f, 35.0f, u0, v0, u0 + 31.0f, v0 + 38.0f, white);
      x += 25.0f;
    }
  }

  if (phase_ == Phase::kCounting) {
    for (std::size_t row = 0; row < started_.size() && started_[row]; ++row) {
      const auto elapsed = static_cast<float>(phase_frames_ - start_time_[row]);
      draw_row(out, static_cast<int>(row), {1.0f, 1.0f, 1.0f, std::min(1.0f, elapsed * kRowFadePerFrame)}, false);
    }
  } else if (phase_ == Phase::kHold) {
    for (std::size_t row = 0; row < started_.size(); ++row) draw_row(out, static_cast<int>(row), white, false);
  } else {
    for (std::size_t row = 0; row < started_.size(); ++row) draw_row(out, static_cast<int>(row), white, true);
    if (battle_) {
      // A tie shows neither.
      if (players_[0].points > players_[1].points) {
        draw_sprite(out, kBattleLeftWins, white);
        draw_sprite(out, kBattleRightLoses, white);
      } else if (players_[0].points < players_[1].points) {
        draw_sprite(out, kBattleRightWins, white);
        draw_sprite(out, kBattleLeftLoses, white);
      }
      return;
    }
    for (std::size_t p = 0; p < 2; ++p) {
      if (!players_[p].active) continue;
      const LetterRange& letter = kLetters[p][letter_index(players_[p].grade)];
      for (int id = letter.first; id < letter.first + letter.count; ++id) draw_sprite(out, id, white);
    }
  }
}

}  // namespace piu
