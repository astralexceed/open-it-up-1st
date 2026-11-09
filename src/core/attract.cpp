// Timing, coin handling and drawing of the attract loop.

#include "core/attract.h"

#include <algorithm>
#include <array>

namespace piu {
namespace {

constexpr int kStepsPerFrame = 4;

constexpr int kAndamiroFadeStart = 0x16c;
constexpr int kAndamiroEnd = 0x1a8;
constexpr float kAndamiroFade = 0.02f;

// INTRO.ANI, stored by the extractor as one sheet.
constexpr int kIntroFrames = 304;
constexpr int kIntroColumns = 16;
constexpr float kIntroFrameW = 160.0f;
constexpr float kIntroFrameH = 120.0f;
constexpr int kIntroFramesPerImage = 2;

constexpr int kTitleFirst = 0x70;
constexpr int kTitleWrap = 0x345;
constexpr int kTitleFadeStart = 599;
constexpr float kTitleFade = 0.04f;
constexpr int kJoinWaitFrames = 0x78;

constexpr int kDemoSongs = 10;

}  // namespace

AttractLoop::AttractLoop(const Bga& andamiro, const Bga& start, const Assets& assets, CoinBox& coins,
                         const MachineSettings& settings)
    : coins_(coins), demo_sound_(settings.demo_sound != 0), andamiro_(andamiro, assets, missing_),
      start_(start, assets, missing_), hud_(assets) {
  if (const Texture* texture = assets.texture("intro")) intro_ = *texture;
  if (const Texture* texture = assets.texture("white")) white_ = *texture;
  // The machine starts on the logo.
  timer_ = 0;
}

void AttractLoop::press_center(int player) {
  if (phase_ == Phase::kTitle && (player == 0 || player == 1)) pressed_[static_cast<std::size_t>(player)] = true;
}

void AttractLoop::step() {
  if (ticks_++ % kStepsPerFrame == 0) run_frame();
}

std::optional<AttractLoop::Event> AttractLoop::take_event() {
  if (events_.empty()) return std::nullopt;
  const Event event = events_.front();
  events_.erase(events_.begin());
  return event;
}

void AttractLoop::run_frame() {
  ++frame_count_;
  switch (phase_) {
    case Phase::kAndamiro: run_andamiro(); break;
    case Phase::kIntro: run_intro(); break;
    case Phase::kTitle: run_title(); break;
    default: break;
  }
}

void AttractLoop::run_andamiro() {
  // About 7 seconds, or until there is a coin.
  ++timer_;
  if (timer_ > kAndamiroFadeStart) brightness_ = std::max(0.0f, brightness_ - kAndamiroFade);
  if (timer_ > kAndamiroEnd || coins_.coins() != 0) enter_intro_or_title();
}

void AttractLoop::enter_intro_or_title() {
  // The intro only plays on a machine without coins.
  if (coins_.coins() != 0) {
    enter_title();
    return;
  }
  phase_ = Phase::kIntro;
  intro_frame_ = 0;
  timer_ = 0;
  if (demo_sound_) events_.push_back(Event::kIntroMusicStart);
}

void AttractLoop::run_intro() {
  // One image every two frames, 304 images.
  if (coins_.coins() != 0) {
    if (demo_sound_) events_.push_back(Event::kIntroMusicStop);
    enter_title();
    return;
  }
  if (++timer_ >= kIntroFramesPerImage) {
    timer_ = 0;
    if (++intro_frame_ >= kIntroFrames) {
      if (demo_sound_) events_.push_back(Event::kIntroMusicStop);
      enter_title();
    }
  }
}

void AttractLoop::enter_title() {
  phase_ = Phase::kTitle;
  title_time_ = kTitleFirst;
  title_fade_ = 1.0f;
  title_brightness_ = 1.0f;
  title_fading_ = false;
  has_coin_ = false;
  pressed_ = {};
  joined_ = {};
  join_wait_ = kJoinWaitFrames;
}

void AttractLoop::run_title() {
  // The screen times out only while the machine has no coins.
  if (coins_.coins() != 0) has_coin_ = true;
  if (!has_coin_) {
    if (title_time_ > kTitleFadeStart) title_fading_ = true;
  } else {
    const auto try_join = [&](std::size_t p) {
      if (pressed_[p] && !joined_[p] && coins_.can_afford_one() && coins_.consume()) {
        joined_[p] = true;
        pressed_[p] = false;
        sounds_.play(sfx::kStart, 1);
      }
    };
    if (coins_.can_afford_one()) {
      try_join(0);
      try_join(1);
    }
    // With one player in, the other has two seconds to join.
    for (std::size_t p = 0; p < 2; ++p) {
      const std::size_t other = 1 - p;
      if (joined_[p] && !joined_[other]) {
        join_wait_ = coins_.can_afford_one() ? join_wait_ - 1 : 0;
        if (pressed_[p]) {
          join_wait_ = 0;
          sounds_.play(sfx::kStart, 1);
        }
        if (join_wait_ < 1) {
          events_.push_back(Event::kPlayersReady);
          phase_ = Phase::kDone;
          return;
        }
      }
    }
    if (joined_[0] && joined_[1]) {
      events_.push_back(Event::kPlayersReady);
      phase_ = Phase::kDone;
      return;
    }
  }
  pressed_ = {};

  ++title_time_;
  if (title_time_ > kTitleWrap) title_time_ = kTitleFirst;

  if (title_fading_) {
    title_brightness_ = title_fade_;
    title_fade_ -= kTitleFade;
    if (title_fade_ < 0.0f) {
      // Nobody came: the demo is next. The song list wraps after ten.
      demo_index_ = next_demo_;
      next_demo_ = (next_demo_ + 1) % kDemoSongs;
      events_.push_back(Event::kStartDemo);
      phase_ = Phase::kDemo;
    }
  }
}

void AttractLoop::demo_finished() {
  // After the demo the machine returns to the logo.
  phase_ = Phase::kAndamiro;
  timer_ = 0;
  brightness_ = 1.0f;
}

void AttractLoop::draw(DrawList& out) const {
  switch (phase_) {
    case Phase::kAndamiro:
      andamiro_.draw(out, static_cast<float>(timer_), brightness_);
      break;
    case Phase::kIntro: {
      if (intro_.valid()) {
        const int index = std::min(intro_frame_, kIntroFrames - 1);
        const auto u0 = static_cast<float>(index % kIntroColumns) * kIntroFrameW;
        const auto v0 = static_cast<float>(index / kIntroColumns) * kIntroFrameH;
        emit_quad(out, intro_, 0.0f, 0.0f, 640.0f, 480.0f, u0, v0, u0 + kIntroFrameW, v0 + kIntroFrameH, kIdentity,
                  {1.0f, 1.0f, 1.0f, 1.0f}, 0);
      }
      // Each source pixel becomes four screen pixels: three lit and
      // one black, on every other line. The gaps stay black.
      if (white_.valid()) {
        const Color black = {0.0f, 0.0f, 0.0f, 1.0f};
        const auto bar = [&](float x, float y, float w, float h) {
          out.quad(white_.id, {x, y, 0.5f, 0.5f, black}, {x + w, y, 0.5f, 0.5f, black},
                   {x + w, y + h, 0.5f, 0.5f, black}, {x, y + h, 0.5f, 0.5f, black});
        };
        for (int y = 1; y < 480; y += 2) bar(0.0f, static_cast<float>(y), 640.0f, 1.0f);
        for (int x = 3; x < 640; x += 4) bar(static_cast<float>(x), 0.0f, 1.0f, 480.0f);
      }
      break;
    }
    case Phase::kTitle:
      start_.draw(out, static_cast<float>(title_time_), title_brightness_);
      hud_.draw(out, coins_, CreditHud::Screen::kTitle, joined_[0], joined_[1], false, frame_count_);
      break;
    default:
      break;
  }
}

}  // namespace piu
