// The key mapping screen: its own small font, the pads, and what each click and key does.

#include "core/key_map_screen.h"

#include <array>
#include <cmath>

#include "core/bga_player.h"

namespace piu {
namespace {

// A 5x7 font for ASCII 32 to 95, one row per byte.
constexpr std::array<std::array<std::uint8_t, 7>, 64> kGlyphs = {{
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {{0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
    {{0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00}},
    {{0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x08}},
    {{0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {{0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}},
    {{0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {{0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {{0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {{0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {{0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {{0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {{0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {{0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {{0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {{0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {{0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    {{0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {{0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {{0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {{0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {{0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {{0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {{0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {{0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {{0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {{0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
    {{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {{0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {{0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {{0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {{0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {{0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {{0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {{0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {{0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {{0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {{0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}},
    {{0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {{0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04}},
    {{0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
    {{0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}},
    {{0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01}},
    {{0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
}};

constexpr int kScale = 4;
constexpr int kCellW = 24;
constexpr int kCellH = 32;
constexpr int kColumns = 16;
constexpr int kRows = 4;
constexpr int kAtlasW = kCellW * kColumns;
constexpr int kShapesY = kCellH * kRows;
constexpr int kShapeSize = 64;
constexpr int kAtlasH = kShapesY + kShapeSize;
constexpr int kArrowX = 0;
constexpr int kRingX = kShapeSize;
constexpr int kWhiteX = kShapeSize * 2;

constexpr Color kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
constexpr Color kBlack = {0.0f, 0.0f, 0.0f, 1.0f};
constexpr Color kSoft = {0.66f, 0.63f, 0.77f, 1.0f};
constexpr Color kBack = {0.09f, 0.08f, 0.16f, 1.0f};
constexpr Color kPad = {0.16f, 0.15f, 0.27f, 1.0f};
constexpr Color kButton = {0.21f, 0.19f, 0.35f, 1.0f};
constexpr Color kSky = {0.53f, 0.81f, 0.92f, 1.0f};
constexpr Color kWarn = {1.0f, 0.54f, 0.36f, 1.0f};
constexpr Color kRed = {0.87f, 0.16f, 0.17f, 1.0f};
constexpr Color kYellow = {0.98f, 0.81f, 0.13f, 1.0f};
constexpr Color kBlue = {0.15f, 0.42f, 0.93f, 1.0f};

constexpr float kPanelSize = 66.0f;
constexpr float kPanelStep = 74.0f;
constexpr float kPadY = 186.0f;
constexpr std::array<float, 2> kPadX = {166.0f, 474.0f};
// Column and row of each panel around the middle of its pad.
constexpr std::array<std::array<int, 2>, 5> kPanelPlace = {{{-1, 1}, {-1, -1}, {0, 0}, {1, -1}, {1, 1}}};
// Quarter turns of the arrow, which points up-left.
constexpr std::array<int, 5> kPanelTurn = {1, 0, 0, 3, 2};

constexpr float kSmallTop = 330.0f;
constexpr float kSmallW = 116.0f;
constexpr float kSmallH = 56.0f;
constexpr float kSmallLeft = 18.0f;
constexpr float kSmallStep = 121.0f;

constexpr std::array<const char*, 5> kPanelEn = {"DOWN-LEFT", "UP-LEFT", "CENTER", "UP-RIGHT", "DOWN-RIGHT"};
constexpr std::array<const char*, 5> kPanelEs = {"ABAJO IZQ", "ARRIBA IZQ", "CENTRO", "ARRIBA DER", "ABAJO DER"};
constexpr std::array<const char*, 5> kOtherEn = {"COIN 1", "COIN 2", "TEST", "SERVICE", "CLEAR"};
constexpr std::array<const char*, 5> kOtherEs = {"MONEDA 1", "MONEDA 2", "TEST", "SERVICIO", "CLEAR"};

float text_width(float scale, std::size_t letters) { return letters == 0 ? 0.0f : (static_cast<float>(letters) * 6.0f - 1.0f) * scale; }

Color darker(Color color) { return {color.r * 0.62f, color.g * 0.62f, color.b * 0.62f, 1.0f}; }

}  // namespace

KeyMapAtlas make_key_map_atlas() {
  KeyMapAtlas atlas{kAtlasW, kAtlasH, std::vector<std::uint8_t>(static_cast<std::size_t>(kAtlasW) * kAtlasH * 4, 0)};
  const auto set = [&](int x, int y) {
    std::uint8_t* pixel = &atlas.rgba[(static_cast<std::size_t>(y) * kAtlasW + static_cast<std::size_t>(x)) * 4];
    pixel[0] = pixel[1] = pixel[2] = pixel[3] = 255;
  };
  for (int c = 0; c < 64; ++c) {
    const int cell_x = (c % kColumns) * kCellW;
    const int cell_y = (c / kColumns) * kCellH;
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((kGlyphs[static_cast<std::size_t>(c)][static_cast<std::size_t>(row)] >> (4 - column) & 1) == 0) continue;
        for (int y = 0; y < kScale; ++y) {
          for (int x = 0; x < kScale; ++x) set(cell_x + 2 + column * kScale + x, cell_y + 2 + row * kScale + y);
        }
      }
    }
  }
  for (int y = 0; y < kShapeSize; ++y) {
    for (int x = 0; x < kShapeSize; ++x) {
      // An arrow pointing up-left: a triangular head and a diagonal shaft.
      const bool head = x >= 8 && y >= 8 && x + y < 52;
      const bool shaft = std::abs(x - y) < 9 && x + y >= 40 && x + y < 106;
      if (head || shaft) set(kArrowX + x, kShapesY + y);
      const float distance = std::hypot(static_cast<float>(x) - 31.5f, static_cast<float>(y) - 31.5f);
      if (distance > 14.0f && distance < 25.0f) set(kRingX + x, kShapesY + y);
      set(kWhiteX + x, kShapesY + y);
    }
  }
  return atlas;
}

KeyMapScreen::KeyMapScreen(const KeyMap& keys, bool spanish, const Texture& atlas) : keys_(keys), spanish_(spanish), atlas_(atlas) {}

KeyMapScreen::Rect KeyMapScreen::target_rect(int action) {
  if (action >= kKeyCoin1) {
    return {kSmallLeft + static_cast<float>(action - kKeyCoin1) * kSmallStep, kSmallTop, kSmallW, kSmallH};
  }
  const auto& place = kPanelPlace[static_cast<std::size_t>(action % 5)];
  return {kPadX[static_cast<std::size_t>(action / 5)] + static_cast<float>(place[0]) * kPanelStep - kPanelSize / 2.0f,
          kPadY + static_cast<float>(place[1]) * kPanelStep - kPanelSize / 2.0f, kPanelSize, kPanelSize};
}

std::string KeyMapScreen::action_name(int action) const {
  if (action >= kKeyCoin1) return (spanish_ ? kOtherEs : kOtherEn)[static_cast<std::size_t>(action - kKeyCoin1)];
  const std::string player = action < 5 ? (spanish_ ? "J1 " : "P1 ") : (spanish_ ? "J2 " : "P2 ");
  return player + (spanish_ ? kPanelEs : kPanelEn)[static_cast<std::size_t>(action % 5)];
}

void KeyMapScreen::press_key(const std::string& code) {
  if (code == "Escape") {
    if (selected_ >= 0) {
      selected_ = -1;
      conflict_ = -1;
    } else {
      result_ = Result::kCancelled;
    }
    return;
  }
  if (selected_ < 0 || key_label(code).empty()) return;
  const int owner = keys_.action_for(code);
  if (owner >= 0 && owner != selected_) {
    conflict_ = owner;
    return;
  }
  keys_.assign(selected_, code);
  selected_ = -1;
  conflict_ = -1;
}

void KeyMapScreen::click(float x, float y) {
  conflict_ = -1;
  for (int action = 0; action < static_cast<int>(kKeyActions); ++action) {
    if (target_rect(action).contains(x, y)) {
      selected_ = action;
      return;
    }
  }
  selected_ = -1;
  if (Rect{18.0f, 418.0f, 190.0f, 44.0f}.contains(x, y)) keys_ = KeyMap();
  else if (Rect{432.0f, 418.0f, 190.0f, 44.0f}.contains(x, y)) result_ = Result::kSaved;
}

void KeyMapScreen::fill(DrawList& out, const Rect& rect, Color color) const {
  const auto u = static_cast<float>(kWhiteX);
  const auto v = static_cast<float>(kShapesY);
  emit_quad(out, atlas_, rect.x, rect.y, rect.w, rect.h, u + 8.0f, v + 8.0f, u + 16.0f, v + 16.0f, kIdentity, color, 0);
}

void KeyMapScreen::outline(DrawList& out, const Rect& rect, Color color) const {
  constexpr float t = 3.0f;
  fill(out, {rect.x - t, rect.y - t, rect.w + t * 2.0f, t}, color);
  fill(out, {rect.x - t, rect.y + rect.h, rect.w + t * 2.0f, t}, color);
  fill(out, {rect.x - t, rect.y, t, rect.h}, color);
  fill(out, {rect.x + rect.w, rect.y, t, rect.h}, color);
}

void KeyMapScreen::write(DrawList& out, float x, float y, float scale, const std::string& value, Color color) const {
  for (const char c : value) {
    if (c >= 33 && c <= 95) {
      const int index = c - 32;
      const auto u = static_cast<float>(index % kColumns * kCellW + 2);
      const auto v = static_cast<float>(index / kColumns * kCellH + 2);
      emit_quad(out, atlas_, x, y, 5.0f * scale, 7.0f * scale, u, v, u + 20.0f, v + 28.0f, kIdentity, color, 0);
    }
    x += 6.0f * scale;
  }
}

void KeyMapScreen::write_centered(DrawList& out, const Rect& rect, float scale, const std::string& value, Color color) const {
  write(out, rect.x + (rect.w - text_width(scale, value.size())) / 2.0f, rect.y + (rect.h - 7.0f * scale) / 2.0f, scale, value, color);
}

void KeyMapScreen::draw_panel(DrawList& out, int action) const {
  const int panel = action % 5;
  const Rect rect = target_rect(action);
  const Color color = panel == 2 ? kYellow : panel == 1 || panel == 3 ? kRed : kBlue;
  fill(out, rect, color);
  const auto shape_x = static_cast<float>(panel == 2 ? kRingX : kArrowX);
  const auto shape_y = static_cast<float>(kShapesY);
  emit_quad(out, atlas_, rect.x + 5.0f, rect.y + 5.0f, rect.w - 10.0f, rect.h - 10.0f, shape_x, shape_y, shape_x + kShapeSize, shape_y + kShapeSize,
            kIdentity, darker(color), kPanelTurn[static_cast<std::size_t>(panel)]);
  const std::string label = key_label(keys_.code(action));
  write_centered(out, rect, label.size() > 3 ? 2.0f : 3.0f, label, panel == 2 ? kBlack : kWhite);
}

void KeyMapScreen::draw_button(DrawList& out, int action) const {
  const Rect rect = target_rect(action);
  fill(out, rect, kButton);
  write_centered(out, {rect.x, rect.y + 5.0f, rect.w, 16.0f}, 2.0f, action_name(action), kSoft);
  write_centered(out, {rect.x, rect.y + 24.0f, rect.w, 26.0f}, 3.0f, key_label(keys_.code(action)), kWhite);
}

void KeyMapScreen::draw(DrawList& out) const {
  ++frames_;
  fill(out, {0.0f, 0.0f, kScreenWidth, kScreenHeight}, kBack);
  write_centered(out, {0.0f, 12.0f, kScreenWidth, 26.0f}, 3.0f, text("MAP CONTROLS", "MAPEAR CONTROLES"), kSky);

  for (std::size_t pad = 0; pad < 2; ++pad) {
    const Rect box = {kPadX[pad] - 126.0f, kPadY - 136.0f, 252.0f, 250.0f};
    fill(out, box, kPad);
    write_centered(out, {box.x, box.y + 6.0f, box.w, 16.0f}, 2.0f, pad == 0 ? text("PLAYER 1", "JUGADOR 1") : text("PLAYER 2", "JUGADOR 2"), kWhite);
  }
  for (int action = 0; action < 10; ++action) draw_panel(out, action);
  for (int action = kKeyCoin1; action < static_cast<int>(kKeyActions); ++action) draw_button(out, action);

  const Rect status = {0.0f, 303.0f, kScreenWidth, 18.0f};
  if (conflict_ >= 0) {
    write_centered(out, status, 2.0f, std::string(text("THAT KEY IS USED BY: ", "ESA TECLA YA LA USA: ")) + action_name(conflict_), kWarn);
    outline(out, target_rect(conflict_), kWarn);
  } else if (selected_ >= 0) {
    write_centered(out, status, 2.0f, std::string(text("PRESS A KEY FOR: ", "PRESIONA UNA TECLA PARA: ")) + action_name(selected_), kYellow);
  } else {
    write_centered(out, status, 2.0f, text("CLICK A CONTROL, THEN PRESS A KEY", "HAZ CLIC EN UN CONTROL Y PRESIONA UNA TECLA"), kSoft);
  }
  if (selected_ >= 0 && frames_ / 15 % 2 == 0) outline(out, target_rect(selected_), kWhite);

  const Rect reset = {18.0f, 418.0f, 190.0f, 44.0f};
  const Rect save = {432.0f, 418.0f, 190.0f, 44.0f};
  fill(out, reset, kButton);
  fill(out, save, kBlue);
  write_centered(out, reset, 2.0f, text("RESET", "RESTABLECER"), kWhite);
  write_centered(out, save, 2.0f, text("SAVE", "GUARDAR"), kWhite);
  write_centered(out, {208.0f, 418.0f, 224.0f, 44.0f}, 2.0f, text("ESC: CANCEL", "ESC: CANCELAR"), kSoft);
}

}  // namespace piu
