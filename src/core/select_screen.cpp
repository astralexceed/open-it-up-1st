// Logic and drawing of the selection and help screens.

#include "core/select_screen.h"

#include <algorithm>
#include <cmath>

#include "core/game.h"

namespace piu {
namespace {

constexpr int kStepsPerLoop = 8;  // The selection loop waits two 60 Hz frames.
constexpr int kStepsPer60 = 4;

constexpr int kNoSelection = 9;
constexpr int kTimeLimit = 0x4b0;
constexpr int kTitleCardFrames = 0x3c;

constexpr int kPanelDownLeft = 0;
constexpr int kPanelUpLeft = 1;
constexpr int kPanelCenter = 2;
constexpr int kPanelUpRight = 3;
constexpr int kPanelDownRight = 4;
constexpr int kPanelsPerPad = 5;

constexpr float kConfirmFade = 1.0f / 30.0f;
constexpr std::array<float, 8> kCdPos = {87, 87, 429, 87, 87, 285, 429, 285};

constexpr int kModeMusicTrack = 4;
constexpr int kSelectMusicTrack = 5;

// Keypad digits of each panel, as the input codes spell them.
constexpr std::array<char, 5> kCodeDigit = {'1', '7', '5', '9', '3'};

}  // namespace

SelectScreen::SelectScreen(const StageConfig& config, const Bga& mode_bga, const Bga& music_bga, const Bga& cd_bga,
                           const Assets& assets, CoinBox& coins, SelectSession& session, bool event_mode)
    : config_(config), coins_(coins), session_(session), mode_(mode_bga, assets, missing_),
      music_(music_bga, assets, missing_), cds_(cd_bga, assets, missing_), hud_(assets) {
  if (const Texture* texture = assets.texture("dec2")) digits_ = *texture;
  if (event_mode && session_.stage == 1) {
    session_.mode = kModeEvent;
    session_.option_words = {};
    enter_music();
  } else if (session_.stage == 1) {
    enter_mode();
  } else {
    enter_music();
  }
}

void SelectScreen::enter_mode() {
  phase_ = Phase::kMode;
  session_.option_words = {};
  slots_ = {kModeEasy, kModeHard, session_.players == 3 ? kModeBattle : kModeDouble, kModeNonstop};
  previous_ = 8;
  selected_ = kNoSelection;
  confirming_ = false;
  t_ = 0;
  confirm_t_ = 0;
  timer_start_ = frames_60_;
  track_ = kModeMusicTrack;
  events_.push_back(Event::kPlayModeMusic);
}

void SelectScreen::enter_music() {
  phase_ = Phase::kMusic;
  const auto& stages = config_.modes[static_cast<std::size_t>(session_.mode)].stages;
  const std::size_t stage = std::min(static_cast<std::size_t>(session_.stage - 1), stages.size() - 1);
  songs_ = stages[stage];
  // Stage 2 replaces its first song if the last one chosen is the song that repeats there.
  if (session_.stage == 2 && session_.previous_song == 3 &&
      (session_.mode == kModeEasy || session_.mode == kModeDouble) && !stages[0].empty() && stages[0].size() > 2) {
    songs_[0] = stages[0][2];
  }
  previous_ = 8;
  selected_ = kNoSelection;
  confirming_ = false;
  t_ = 0;
  confirm_t_ = 0;
  timer_start_ = frames_60_;
  track_ = kSelectMusicTrack;
  events_.push_back(Event::kPlaySelectMusic);
}

void SelectScreen::press(int panel) {
  if (panel >= 0 && panel < 10) pressed_[static_cast<std::size_t>(panel)] = true;
}

std::optional<SelectScreen::Event> SelectScreen::take_event() {
  if (events_.empty()) return std::nullopt;
  const Event event = events_.front();
  events_.erase(events_.begin());
  return event;
}

const StageTrack& SelectScreen::song_track() const {
  const auto cd = static_cast<std::size_t>(song().cd_id);
  return config_.tracks[std::min(cd, config_.tracks.size() - 1)];
}

int SelectScreen::seconds_left() const { return (timer_start_ + kTimeLimit - frames_60_) / 60; }

void SelectScreen::step() {
  if (ticks_ % kStepsPer60 == 0) ++frames_60_;
  if (phase_ == Phase::kTitle && ticks_ % kStepsPer60 == 0 && ++title_frames_ >= kTitleCardFrames) {
    phase_ = Phase::kDone;
    events_.push_back(Event::kDone);
  }
  if (ticks_++ % kStepsPerLoop == 0) run_loop();
}

void SelectScreen::run_loop() {
  if (phase_ != Phase::kMode && phase_ != Phase::kMusic) return;
  update_join();
  update_codes();
  if (phase_ == Phase::kMode) {
    update_mode();
  } else {
    update_music();
  }
  pressed_ = {};
  ++t_;
  if (confirming_) ++confirm_t_;

  if (confirming_ && phase_ == Phase::kMode && confirm_t_ > 30) {
    enter_music();
  } else if (confirming_ && phase_ == Phase::kMusic && confirm_t_ > 30) {
    phase_ = Phase::kWaitTitle;
    selected_song_ = selected_;
    events_.push_back(Event::kStopMusic);
    events_.push_back(Event::kNeedTitle);
  }
}

void SelectScreen::update_join() {
  // The second player can still pay in here.
  if (session_.players == 3) return;
  // While a mode is being picked, a highlighted Double already keeps the second player out.
  const int current_mode = phase_ == Phase::kMode ? selected_ : session_.mode;
  if (current_mode != kModeDouble && coins_.can_afford_one()) {
    join_pressing_players();
  }
  // A second player turns the Double corner into Battle, label included.
  slots_[2] = session_.players == 3 ? kModeBattle : kModeDouble;
}

void SelectScreen::join_pressing_players() {
  if (!(session_.players & 1) && pressed(kPanelCenter) && coins_.consume()) {
    session_.players |= 1;
    sounds_.play(sfx::kStart, 1);
  }
  if (!(session_.players & 2) && pressed(kPanelCenter + kPanelsPerPad) && coins_.consume()) {
    session_.players |= 2;
    sounds_.play(sfx::kStart, 1);
  }
}

void SelectScreen::apply_code(std::size_t player, const std::string& code) {
  // Each code toggles an option; 55555555 clears them.
  std::uint32_t& word = session_.option_words[player];
  if (code == "55555555") word = 0;
  else if (code == "55975315") word ^= 0x100;
  else if (code == "55159357") word ^= 0x200;
  else if (code == "55153555") word ^= 0x70;
  else if (code == "55355755") word ^= 0x30;
  else if (code == "55755595") word ^= 0x10;
  else if (code == "55797955") word = (word & ~0x3000u) | 0x1000;
  else if (code == "55979755") word = (word & ~0x3000u) | 0x2000;
  else if (code == "51535957") word ^= 0x4000;
  else return;
  codes_[player].clear();
  sounds_.play(sfx::kCode, 1);
}

void SelectScreen::update_codes() {
  for (std::size_t player = 0; player < 2; ++player) {
    if (!(session_.players & (1 << player))) continue;
    const int base = static_cast<int>(player) * kPanelsPerPad;
    std::string& code = codes_[player];
    for (int panel = 0; panel < kPanelsPerPad; ++panel) {
      if (pressed(base + panel)) {
        code.push_back(kCodeDigit[static_cast<std::size_t>(panel)]);
        if (code.size() > 8) code.erase(0, code.size() - 8);
      }
    }
    if (code.size() == 8) apply_code(player, code);
  }
}

void SelectScreen::tick_seconds() {
  if (last_seconds_ == ticked_seconds_) return;
  ticked_seconds_ = last_seconds_;
  sounds_.play(sfx::kTick, 5);
}

void SelectScreen::update_mode() {
  // Pressing a corner picks it; pressing it again confirms.
  if (confirming_) return;
  const auto pad = [&](int base) {
    const auto pick = [&](int panel, int mode) {
      if (pressed(base + panel)) {
        previous_ = selected_;
        selected_ = mode;
        sounds_.play(sfx::kMove, 2);
      }
    };
    pick(kPanelUpLeft, kModeEasy);
    pick(kPanelUpRight, kModeHard);
    pick(kPanelDownLeft, session_.players == 3 ? kModeBattle : kModeDouble);
    pick(kPanelDownRight, kModeNonstop);
  };
  if (session_.players & 1) pad(0);
  if (session_.players & 2) pad(kPanelsPerPad);
  last_seconds_ = seconds_left();
  tick_seconds();
  if (selected_ == previous_ || last_seconds_ < 1) {
    if (selected_ == kNoSelection) selected_ = kModeEasy;
    session_.mode = selected_;
    confirming_ = true;
    sounds_.play(sfx::kConfirm, 3);
    confirm_t_ = 0;
  }
}

void SelectScreen::choose_song(int slot) {
  const auto index = static_cast<std::size_t>(slot);
  if (index >= songs_.size()) return;
  previous_ = selected_;
  selected_ = slot;
  sounds_.play(sfx::kMove, 2);
  if (previous_ != selected_) {
    // A choice of the port, not of the machine: the grade's applause does not
    // run under the preview.
    sounds_.stop(3);
    sounds_.stop(6);
    track_ = songs_[index].cd_id + config_.cd_preview_offset;
    events_.push_back(Event::kPreview);
  }
}

void SelectScreen::update_music() {
  // Double reads both pads.
  if (confirming_) return;
  const auto pad = [&](int base) {
    if (pressed(base + kPanelUpLeft)) choose_song(0);
    if (pressed(base + kPanelUpRight)) choose_song(1);
    if (pressed(base + kPanelDownLeft)) choose_song(2);
    if (pressed(base + kPanelDownRight)) choose_song(3);
  };
  if ((session_.players & 1) || session_.mode == kModeDouble) pad(0);
  if ((session_.players & 2) || session_.mode == kModeDouble) pad(kPanelsPerPad);
  last_seconds_ = seconds_left();
  tick_seconds();
  if (selected_ == previous_ || last_seconds_ < 1) {
    if (selected_ == kNoSelection) selected_ = 0;
    session_.previous_song = selected_;
    if (session_.mode == kModeEvent && selected_ >= 2) session_.mode = kModeNonstop;
    confirming_ = true;
    sounds_.play(sfx::kConfirm, 3);
    confirm_t_ = 0;
  }
}

void SelectScreen::title_ready(const Assets& assets) {
  title_.clear();
  if (const Sprite* sprite = assets.sprite(song_track().title)) {
    for (const SpriteQuad& q : sprite->quads) {
      const Texture* texture = assets.texture(q.texture);
      title_.push_back({texture != nullptr ? *texture : Texture{}, q.x, q.y, q.w, q.h, q.u0, q.v0, q.u1, q.v1});
    }
  }
  title_frames_ = 0;
  phase_ = Phase::kTitle;
}

void SelectScreen::draw_time(DrawList& out) const {
  if (!digits_.valid()) return;
  const int seconds = std::max(0, last_seconds_);
  const auto digit = [&](int value, float x) {
    const int frame = value + 10;
    const auto u0 = static_cast<float>(32 * (frame % 8));
    const auto v0 = static_cast<float>(39 * (frame / 8));
    emit_quad(out, digits_, x, 5.0f, 32.0f, 39.0f, u0, v0, u0 + 31.0f, v0 + 38.0f, kIdentity, {1, 1, 1, 1}, 0);
  };
  digit(seconds / 10, 0x22c);
  digit(seconds % 10, 0x248);
}

void SelectScreen::draw_icons(DrawList& out) const {
  // A fixed place per option, on each player's side.
  for (std::size_t player = 0; player < 2; ++player) {
    const std::uint32_t word = session_.option_words[player];
    const float x = player == 0 ? 8.0f : 0x255;
    const auto icon = [&](int id, float y) { hud_.draw_sprite_at(out, id, x, y); };
    if (word & 0x70) icon(0x3f, 0xb7);
    if (word & 0x100) icon(0x40, 0xdf);
    if (word & 0x200) icon(0x41, 0x107);
    if (word & 0x1000) icon(0x42, 0x12f);
    if (word & 0x2000) icon(0x3c, 0x157);
  }
}

void SelectScreen::draw_mode(DrawList& out) const {
  const float fade = confirming_ ? static_cast<float>(30 - confirm_t_) * kConfirmFade : 1.0f;
  mode_.draw(out, static_cast<float>(t_ % 0x2d0 + 0x79), fade);

  for (int slot : slots_) {
    if (slot == selected_) {
      mode_.draw_layer(out, slot + 0x11, static_cast<float>(confirm_t_ % 0x1e + 0x619), 1.0f);
      break;
    }
  }

  const auto fx = static_cast<float>(t_ % 0x2d0 + 0x619);
  if (!confirming_) {
    for (int layer : {10, 11, 12, 13}) mode_.draw_layer(out, layer, fx, 1.0f);
    for (int layer : {0x20, 0x21, 0x22}) mode_.draw_layer(out, layer, 0x619, 1.0f);
    const auto blink = static_cast<float>(t_ % 0x3c + 0x619);
    for (int slot : slots_) {
      mode_.draw_layer(out, slot + 0x18, slot == selected_ ? blink : 0x619, 1.0f);
    }
    draw_time(out);
    mode_.draw_layer(out, 0x1f, static_cast<float>(t_ % 0x1e + 0x619), 1.0f);
    mode_.draw_layer(out, 0x24, 0x619, 1.0f);
    const bool valid = selected_ >= 0 && selected_ <= 4;
    mode_.draw_layer(out, valid ? 0x26 : 0x25, valid ? 0x619 : static_cast<float>(t_ % 0x1e + 0x619), 1.0f);
    draw_icons(out);
    const int p = session_.players;
    hud_.draw(out, coins_, CreditHud::Screen::kSelect, (p & 1) != 0, (p & 2) != 0, false, t_ * 2);
  } else {
    for (int slot : slots_) {
      if (slot == selected_) continue;
      for (int layer : {10, 11, 12, 13}) mode_.draw_layer(out, layer, fx, fade);
      mode_.draw_layer(out, slot + 0x18, 0x637, fade);
    }
    if (selected_ >= 0) mode_.draw_layer(out, selected_ + 0x18, static_cast<float>(confirm_t_ + 0x709), 1.0f);
  }
}

void SelectScreen::draw_music(DrawList& out) const {
  const int mode = session_.mode;
  music_.draw(out, static_cast<float>(t_ % 600 + 0x78), 1.0f);
  music_.draw_layer(out, mode == kModeEvent ? 4 : mode + 4, 0x4b1, 1.0f);
  // In EVENT this lands on a layer with nothing to show yet, so that screen has no title.
  music_.draw_layer(out, mode + 0xd, static_cast<float>(t_ % 600 + 0x4b1), 1.0f);

  if (confirming_) {
    if (selected_ >= 0 && selected_ < static_cast<int>(songs_.size())) {
      const float time = confirm_t_ > 0x1d ? 0x78 : static_cast<float>(confirm_t_ + 0x5a);
      cds_.draw_layer(out, songs_[static_cast<std::size_t>(selected_)].cd_id, time, 1.0f, 257.0f, 188.0f);
    }
    return;
  }

  music_.draw(out, static_cast<float>(t_ % 600 + 0xe11), 1.0f);
  music_.draw(out, 0x12c1, 1.0f);
  for (std::size_t i = 0; i < songs_.size() && i < 4; ++i) {
    const float time = static_cast<int>(i) == selected_ ? static_cast<float>(t_ % 0x3c) : 1.0f;
    cds_.draw_layer(out, songs_[i].cd_id, time, 1.0f, kCdPos[i * 2], kCdPos[i * 2 + 1]);
  }
  draw_time(out);
  music_.draw(out, static_cast<float>(t_ % 600 + 0x961), 1.0f);

  if (mode != kModeBattle) {
    // One dot per level around each CD; the selected CD's ring breathes.
    const int phase = t_ % 60;
    const float ring = 100.0f - static_cast<float>(phase < 30 ? phase : 60 - phase) * 0.5f;
    const auto dot_time = static_cast<float>(t_ % 0x1e + 0x3c1);
    for (std::size_t i = 0; i < songs_.size() && i < 4; ++i) {
      const float radius = static_cast<int>(i) == selected_ ? ring : 100.0f;
      for (int level = 0; level < songs_[i].level; ++level) {
        const float angle = static_cast<float>(level) * 0.22f + 0.9f;
        const float sx = std::sin(angle) * radius;
        const float cx = kCdPos[i * 2] + 64.0f;
        const float y = kCdPos[i * 2 + 1] + 64.0f + std::cos(angle) * radius;
        music_.draw_layer(out, 9, dot_time, 1.0f, i % 2 == 0 ? cx + sx : cx - sx, y);
      }
    }
  }

  music_.draw_layer(out, 0x24, 0x871, 1.0f);
  music_.draw(out, static_cast<float>(t_ % 0x1e + 0x745), 1.0f);
  const bool valid = selected_ >= 0 && selected_ <= 3;
  music_.draw_layer(out, valid ? 0x25 : 0x26, static_cast<float>(t_ % 0x3c + 0x871), 1.0f);
  draw_icons(out);
  const int p = session_.players;
  hud_.draw(out, coins_, CreditHud::Screen::kSelect, (p & 1) != 0, (p & 2) != 0, mode == kModeDouble, t_ * 2);
}

void SelectScreen::draw(DrawList& out) const {
  switch (phase_) {
    case Phase::kMode: draw_mode(out); break;
    case Phase::kMusic: draw_music(out); break;
    case Phase::kTitle:
    case Phase::kDone:
      for (const TitleQuad& q : title_) {
        if (q.texture.valid()) {
          emit_quad(out, q.texture, q.x, q.y, q.w, q.h, q.u0, q.v0, q.u1, q.v1, kIdentity, {1, 1, 1, 1}, 0);
        }
      }
      break;
    default: break;
  }
}

HelpScreen::HelpScreen(const Bga& bga, const Assets& assets, const CoinBox& coins, int players)
    : coins_(coins), bga_(bga, assets, missing_), hud_(assets), players_(players) {}

void HelpScreen::press_center(int player) {
  if (player >= 0 && player < 2) pressed_[static_cast<std::size_t>(player)] = true;
}

void HelpScreen::step() {
  if (ticks_++ % kStepsPer60 == 0) run_frame();
}

void HelpScreen::run_frame() {
  // About 22 seconds, or the center panel of a player.
  ++timer_;
  ++frame_;
  if (timer_ > 0x527 && state_ == 0) state_ = 1;
  const bool skip = ((players_ & 1) && pressed_[0]) || ((players_ & 2) && pressed_[1]);
  if (skip && state_ == 0) {
    state_ = 1;
    sounds_.play(sfx::kStart, 1);
  }
  pressed_ = {};
  if (state_ == 1) {
    brightness_ -= 0.02f;
    if (brightness_ <= 0.0f) state_ = 2;
  }
}

void HelpScreen::draw(DrawList& out) const {
  bga_.draw(out, static_cast<float>(timer_), std::max(0.0f, brightness_));
  hud_.draw(out, coins_, CreditHud::Screen::kHelp, (players_ & 1) != 0, (players_ & 2) != 0, false, frame_);
}

void request_select_assets(const Bga& mode_bga, const Bga& music_bga, const Bga& cd_bga, const Bga& help_bga, Assets& assets) {
  request_song_assets(mode_bga, assets);
  request_song_assets(music_bga, assets);
  request_song_assets(cd_bga, assets);
  request_song_assets(help_bga, assets);
}

}  // namespace piu
