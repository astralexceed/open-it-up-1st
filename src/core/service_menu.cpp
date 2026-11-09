// Navigation, options and drawing of every service menu screen.

#include "core/service_menu.h"

#include <cmath>
#include <cstdio>

namespace piu {
namespace {

constexpr int kWidth = 640;
constexpr float kGlyphAdvance = 8.0f;
constexpr float kGlyphHeight = 16.0f;
constexpr int kGridStep = 16;

constexpr float kTitleY = 10.0f;
constexpr float kFirstRowY = 150.0f;
constexpr float kRowStep = 20.0f;
constexpr float kHintMoveY = 420.0f;
constexpr float kHintSelectY = 440.0f;
constexpr float kVersionY = 460.0f;
constexpr float kLabelX = 100.0f;
constexpr float kValueX = 320.0f;
constexpr float kCenterX = 320.0f;
constexpr float kSoundValueX = 400.0f;
constexpr float kEepromValueX = 256.0f;

constexpr Color kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
constexpr Color kRed = {1.0f, 0.0f, 0.0f, 1.0f};
constexpr Color kYellow = {1.0f, 1.0f, 0.0f, 1.0f};

constexpr int kMaxTrack = 14;
constexpr int kEffects = 0x25;

const std::string kMainRows[7] = {"I/O TEST",    "EEPROM TEST", "SCREEN TEST", "GAME OPTION",
                                  "COIN OPTION", "SOUND TEST",  "EXIT"};
const std::string kGameRows[9] = {"GAME MODE",  "DIFFICULTY",      "STAGE BREAK",   "SHOW HELP", "DEMO SOUND",
                                  "DEFAULT SETTING", "CLEAR RANKING", "SAVE AND EXIT", "EXIT"};
const std::string kCoinRows[4] = {"COIN SETTING", "DEFAULT SETTING", "SAVE AND EXIT", "EXIT"};
const std::string kSoundRows[3] = {"CD", "EFFECT", "EXIT"};

const char* const kGameMode[2] = {"NORMAL", "EVENT"};
const char* const kDifficulty[3] = {"EASY", "NORMAL", "HARD"};
const char* const kStageBreak[3] = {"1 STAGE", "2 STAGE", "3 STAGE"};
const char* const kShowHelp[3] = {"ALL", "EASY ONLY", "EASY AND NORMAL"};
const char* const kDemoSound[2] = {"Off", "On"};
const char* const kExitHint = "SERVICE COIN BUTTON TO EXIT";

// Column of each pad on the I/O TEST screen; one sensor strip per side.
struct PanelBox {
  float x, y;
};
constexpr std::array<PanelBox, 5> kPanelBoxes = {{{0x28, 0xa0}, {0xc8, 0xa0}, {0x78, 0xf0}, {0x28, 0x140}, {0xc8, 0x140}}};
// Engine panel shown in each box above. Engine order: down-left, up-left, center, up-right, down-right.
constexpr std::array<int, 5> kPanelOrder = {1, 3, 2, 0, 4};

}  // namespace

void request_service_assets(Assets& assets) {
  assets.request_texture("hfont1");
  assets.request_texture("white");
}

ServiceMenu::ServiceMenu(MachineSettings settings, const Assets& assets) : settings_(settings), saved_(settings) {
  if (const Texture* texture = assets.texture("hfont1")) font_ = *texture;
  if (const Texture* texture = assets.texture("white")) white_ = *texture;
}

int ServiceMenu::rows() const {
  switch (page_) {
    case Page::kMain: return 7;
    case Page::kGame: return 9;
    case Page::kCoin: return 4;
    case Page::kSound: return 3;
    default: return 0;
  }
}

void ServiceMenu::press_test() {
  // The cursor only moves, it never selects.
  const int count = rows();
  if (count == 0) return;
  int& c = cursor();
  c = c + 1 >= count ? 0 : c + 1;
}

void ServiceMenu::press_service() {
  switch (page_) {
    case Page::kMain: {
      constexpr std::array<Page, 6> kScreens = {Page::kIo, Page::kEeprom, Page::kScreen,
                                                Page::kGame, Page::kCoin,  Page::kSound};
      const int c = cursor();
      if (c < 6) {
        page_ = kScreens[static_cast<std::size_t>(c)];
      } else {
        exited_ = true;
      }
      break;
    }
    case Page::kGame: select_game_option(); break;
    case Page::kCoin: select_coin_option(); break;
    case Page::kSound: select_sound_option(); break;
    default: page_ = Page::kMain; break;
  }
}

void ServiceMenu::set_panel(int panel, bool down) {
  if (panel >= 0 && panel < static_cast<int>(panels_.size())) panels_[static_cast<std::size_t>(panel)] = down;
}

void ServiceMenu::reload() {
  settings_ = saved_;
  if (!settings_.valid()) {
    settings_ = MachineSettings{};
    saved_ = settings_;
  }
}

void ServiceMenu::select_game_option() {
  const auto cycle = [](int& value, int count) { value = value + 1 >= count ? 0 : value + 1; };
  switch (cursor()) {
    case 0: cycle(settings_.game_mode, 2); break;
    case 1: cycle(settings_.difficulty, 3); break;
    case 2: cycle(settings_.stage_break, 3); break;
    case 3: cycle(settings_.show_help, 3); break;
    case 4: cycle(settings_.demo_sound, 2); break;
    case 5: settings_.restore_game_defaults(); break;
    case 6:
      // CLEAR RANKING: the machine erases a ranking that the game never shows, so nothing changes on screen.
      break;
    case 7:
      saved_ = settings_;
      page_ = Page::kMain;
      break;
    case 8:
      reload();
      page_ = Page::kMain;
      break;
  }
}

void ServiceMenu::select_coin_option() {
  switch (cursor()) {
    case 0:
      settings_.coins_per_credit = settings_.coins_per_credit >= 10 ? 1 : settings_.coins_per_credit + 1;
      break;
    case 1: settings_.coins_per_credit = 5; break;
    case 2:
      saved_ = settings_;
      page_ = Page::kMain;
      break;
    case 3:
      reload();
      page_ = Page::kMain;
      break;
  }
}

void ServiceMenu::select_sound_option() {
  switch (cursor()) {
    case 0:
      track_ = track_ >= kMaxTrack ? 1 : track_ + 1;
      track_request_ = track_;
      break;
    case 1:
      effect_ = effect_ + 1 >= kEffects ? 0 : effect_ + 1;
      sounds_.play(effect_, 0, true);
      break;
    case 2:
      // A choice of the port: the machine stops only the CD here and leaves the effect looping.
      sounds_.stop(0);
      track_request_ = 0;
      page_ = Page::kMain;
      break;
  }
}

std::optional<int> ServiceMenu::take_track_request() {
  std::optional<int> request = track_request_;
  track_request_.reset();
  return request;
}

void ServiceMenu::text(DrawList& out, float x, float y, const std::string& value, Align align, Color color) const {
  if (!font_.valid()) return;
  const auto width = static_cast<float>(value.size()) * kGlyphAdvance;
  if (align == Align::kCenter) x -= std::floor(width / 2.0f);
  if (align == Align::kRight) x -= width;
  // 32 columns by 16 rows of 8x16 glyphs.
  for (char c : value) {
    const auto code = static_cast<unsigned char>(c);
    // In texels first, so that a scaled glyph does not pick up the edge of the next one.
    const auto cell_w = static_cast<float>(font_.width) / 32.0f;
    const auto cell_h = static_cast<float>(font_.height) / 16.0f;
    float u0 = static_cast<float>(code % 32) * cell_w;
    float v0 = static_cast<float>(code / 32) * cell_h;
    float u1 = u0 + cell_w;
    float v1 = v0 + cell_h;
    inset_texels(u0, u1);
    inset_texels(v0, v1);
    u0 /= static_cast<float>(font_.width);
    u1 /= static_cast<float>(font_.width);
    v0 /= static_cast<float>(font_.height);
    v1 /= static_cast<float>(font_.height);
    out.quad(font_.id, {x, y, u0, v0, color}, {x + kGlyphAdvance, y, u1, v0, color},
             {x + kGlyphAdvance, y + kGlyphHeight, u1, v1, color}, {x, y + kGlyphHeight, u0, v1, color});
    x += kGlyphAdvance;
  }
}

void ServiceMenu::rect(DrawList& out, float x, float y, float w, float h, Color color) const {
  if (!white_.valid()) return;
  out.quad(white_.id, {x, y, 0.5f, 0.5f, color}, {x + w, y, 0.5f, 0.5f, color}, {x + w, y + h, 0.5f, 0.5f, color},
           {x, y + h, 0.5f, 0.5f, color});
}

void ServiceMenu::outline(DrawList& out, float x, float y, float w, float h, Color color) const {
  rect(out, x, y, w, 1.0f, color);
  rect(out, x, y + h - 1.0f, w, 1.0f, color);
  rect(out, x, y, 1.0f, h, color);
  rect(out, x + w - 1.0f, y, 1.0f, h, color);
}

void ServiceMenu::draw_list_rows(DrawList& out, const std::string* labels, int count, float x, Align align) const {
  const int selected = cursors_[static_cast<std::size_t>(page_)];
  for (int i = 0; i < count; ++i) {
    text(out, x, kFirstRowY + kRowStep * static_cast<float>(i), labels[i], align, i == selected ? kRed : kWhite);
  }
}

void ServiceMenu::draw_title(DrawList& out, const char* title) const {
  text(out, kCenterX, kTitleY, title, Align::kCenter, kWhite);
}

void ServiceMenu::draw_exit_hint(DrawList& out) const {
  text(out, kCenterX, kHintSelectY, kExitHint, Align::kCenter, kWhite);
}

void ServiceMenu::draw_main(DrawList& out) const {
  draw_list_rows(out, kMainRows, 7, kCenterX, Align::kCenter);
  draw_title(out, "GAME SETTING");
  text(out, kCenterX, kHintMoveY, "MOVE - TEST MODE BUTTON", Align::kCenter, kWhite);
  text(out, kCenterX, kHintSelectY, "SELECT - SERVICE BUTTON", Align::kCenter, kWhite);
  text(out, static_cast<float>(kWidth), kVersionY, "VER 0.53.1999.9.31", Align::kRight, kWhite);
}

void ServiceMenu::draw_io(DrawList& out) const {
  // A keyboard key lights the four sensors of its panel together.
  outline(out, 0x23, 0x9b, 0xfa, 0xfa, kWhite);
  outline(out, 0x163, 0x9b, 0xfa, 0xfa, kWhite);
  for (int side = 0; side < 2; ++side) {
    for (std::size_t box = 0; box < kPanelBoxes.size(); ++box) {
      const PanelBox& b = kPanelBoxes[box];
      const float x = b.x + (side == 0 ? 0.0f : 0x140);
      outline(out, x, b.y, 0x50, 0x50, kWhite);
      const auto panel = static_cast<std::size_t>(kPanelOrder[box] + side * 5);
      const bool down = panels_[panel];
      const Color color = down ? kYellow : kWhite;
      const auto strip = [&](float sx, float sy, float w, float h) {
        if (down) {
          rect(out, sx, sy, w, h, color);
        } else {
          outline(out, sx, sy, w, h, color);
        }
      };
      strip(x + 13.0f, b.y + 5.0f, 54.0f, 8.0f);
      strip(x + 13.0f, b.y + 67.0f, 54.0f, 8.0f);
      strip(x + 5.0f, b.y + 13.0f, 8.0f, 54.0f);
      strip(x + 67.0f, b.y + 13.0f, 8.0f, 54.0f);
    }
  }
  text(out, 0xa0, 0x64, "TEST BUTTON", Align::kCenter, test_held_ ? kRed : kWhite);
  text(out, 0x1e0, 0x64, "CLEAR BUTTON", Align::kCenter, clear_held_ ? kRed : kWhite);
  draw_exit_hint(out);
  draw_title(out, "I/O TEST");
}

void ServiceMenu::draw_eeprom(DrawList& out) const {
  // Saved settings go through the browser, so the test always passes.
  const std::string pattern = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  text(out, kLabelX, 0x96, "WRITE ........", Align::kLeft, kWhite);
  text(out, kEepromValueX, 0x96, pattern, Align::kLeft, kYellow);
  text(out, kLabelX, 0xb4, "READ  ........", Align::kLeft, kWhite);
  text(out, kEepromValueX, 0xb4, pattern, Align::kLeft, kYellow);
  text(out, kLabelX, 0xd2, "CHECK ........", Align::kLeft, kWhite);
  text(out, kEepromValueX, 0xd2, "SUCCESS", Align::kLeft, kYellow);
  draw_exit_hint(out);
  draw_title(out, "EEPROM TEST");
}

void ServiceMenu::draw_screen_test(DrawList& out) const {
  // A 16-pixel grid and color ramps.
  for (int x = 0; x < kWidth; x += kGridStep) rect(out, static_cast<float>(x), 0, 1, 480, kWhite);
  for (int y = 0; y < 480; y += kGridStep) rect(out, 0, static_cast<float>(y), static_cast<float>(kWidth), 1, kWhite);
  rect(out, 639, 0, 1, 480, kWhite);
  rect(out, 0, 479, 640, 1, kWhite);
  const std::array<Color, 4> ramps = {Color{1, 0, 0, 1}, Color{0, 1, 0, 1}, Color{0, 0, 1, 1}, Color{1, 1, 1, 1}};
  const float x0 = 0x2f;
  const float x1 = 0x130;
  if (white_.valid()) {
    float y = 0x40;
    for (const Color& to : ramps) {
      const Color black = {0, 0, 0, 1};
      out.quad(white_.id, {x0, y, 0.5f, 0.5f, black}, {x1, y, 0.5f, 0.5f, to}, {x1, y + 0x20, 0.5f, 0.5f, to},
               {x0, y + 0x20, 0.5f, 0.5f, black});
      y += 0x20;
    }
    out.quad(white_.id, {x0, y, 0.5f, 0.5f, kWhite}, {x1, y, 0.5f, 0.5f, {1, 0, 0, 1}},
             {x1, 0x141, 0.5f, 0.5f, {0, 1, 0, 1}}, {x0, 0x141, 0.5f, 0.5f, {0, 0, 1, 1}});
  }
  draw_exit_hint(out);
  draw_title(out, "SCREEN TEST");
}

void ServiceMenu::draw_game(DrawList& out) const {
  draw_list_rows(out, kGameRows, 9, kLabelX, Align::kLeft);
  const auto value = [&](int row, const char* label) {
    text(out, kValueX, kFirstRowY + kRowStep * static_cast<float>(row), label, Align::kLeft, kWhite);
  };
  value(0, kGameMode[settings_.game_mode]);
  value(1, kDifficulty[settings_.difficulty]);
  value(2, kStageBreak[settings_.stage_break]);
  value(3, kShowHelp[settings_.show_help]);
  value(4, kDemoSound[settings_.demo_sound]);
  draw_exit_hint(out);
  draw_title(out, "GAME OPTION");
}

void ServiceMenu::draw_coin(DrawList& out) const {
  draw_list_rows(out, kCoinRows, 4, kLabelX, Align::kLeft);
  std::string label = "1 CREDIT / " + std::to_string(settings_.coins_per_credit) +
                      (settings_.coins_per_credit > 1 ? " COINS" : " COIN");
  text(out, kValueX, kFirstRowY, label, Align::kLeft, kWhite);
  draw_exit_hint(out);
  draw_title(out, "COIN OPTION");
}

void ServiceMenu::draw_sound(DrawList& out) const {
  draw_list_rows(out, kSoundRows, 3, kLabelX, Align::kLeft);
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "TRACK %02d", track_);
  text(out, kSoundValueX, kFirstRowY, buffer, Align::kLeft, kWhite);
  std::snprintf(buffer, sizeof(buffer), "%02d", effect_);
  text(out, kSoundValueX, kFirstRowY + kRowStep, buffer, Align::kLeft, kWhite);
  text(out, kCenterX, kHintMoveY, "MOVE - TEST MODE BUTTON", Align::kCenter, kWhite);
  text(out, kCenterX, kHintSelectY, "SELECT - SERVICE BUTTON", Align::kCenter, kWhite);
  draw_title(out, "SOUND TEST");
}

void ServiceMenu::draw(DrawList& out) const {
  switch (page_) {
    case Page::kMain: draw_main(out); break;
    case Page::kIo: draw_io(out); break;
    case Page::kEeprom: draw_eeprom(out); break;
    case Page::kScreen: draw_screen_test(out); break;
    case Page::kGame: draw_game(out); break;
    case Page::kCoin: draw_coin(out); break;
    case Page::kSound: draw_sound(out); break;
  }
}

}  // namespace piu
