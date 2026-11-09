// The key mapping screen: the two pads, and what each click and key does.

#include "core/key_map_screen.h"

#include <array>
#include <string>

#include "core/ui_atlas.h"

namespace piu {
namespace {

constexpr float kMargin = 32.0f;
constexpr float kPanelSize = 58.0f;
constexpr float kPanelStep = 66.0f;
constexpr float kPadY = 200.0f;
constexpr std::array<float, 2> kPadX = {170.0f, 470.0f};
// Column and row of each panel around the middle of its pad.
constexpr std::array<std::array<int, 2>, 5> kPanelPlace = {{{-1, 1}, {-1, -1}, {0, 0}, {1, -1}, {1, 1}}};
// Quarter turns of the arrow, which points up-left.
constexpr std::array<int, 5> kPanelTurn = {1, 0, 0, 3, 2};

constexpr float kSmallTop = 334.0f;
constexpr float kSmallW = 104.0f;
constexpr float kSmallH = 50.0f;
constexpr float kSmallLeft = kMargin;
constexpr float kSmallStep = 118.0f;

constexpr ui::Box kResetBox = {kMargin, 412.0f, 150.0f, 36.0f};
constexpr ui::Box kSaveBox = {458.0f, 412.0f, 150.0f, 36.0f};

constexpr std::array<const char*, 5> kPanelEn = {"Down-left", "Up-left", "Center", "Up-right", "Down-right"};
constexpr std::array<const char*, 5> kPanelEs = {"Abajo izq.", "Arriba izq.", "Centro", "Arriba der.", "Abajo der."};
constexpr std::array<const char*, 5> kOtherEn = {"Coin 1", "Coin 2", "Test", "Service", "Clear"};
constexpr std::array<const char*, 5> kOtherEs = {"Moneda 1", "Moneda 2", "Test", "Servicio", "Clear"};

Color darker(Color color) { return {color.r * 0.72f, color.g * 0.72f, color.b * 0.72f, 1.0f}; }

ui::Box box_of(float x, float y, float w, float h) { return {x, y, w, h}; }

}  // namespace

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
  if (kResetBox.contains(x, y)) keys_ = KeyMap();
  else if (kSaveBox.contains(x, y)) result_ = Result::kSaved;
}

void KeyMapScreen::outline(DrawList& out, const Rect& rect, float radius, Color color) const {
  ui::rounded(out, atlas_, box_of(rect.x - 3.0f, rect.y - 3.0f, rect.w + 6.0f, rect.h + 6.0f), radius + 3.0f, color);
}

void KeyMapScreen::draw_panel(DrawList& out, int action) const {
  const int panel = action % 5;
  const Rect rect = target_rect(action);
  const Color color = panel == 2 ? ui::kYellow : panel == 1 || panel == 3 ? ui::kRed : ui::kBlue;
  const Color letters = panel == 1 || panel == 3 ? ui::kInk : ui::kOnLight;
  if (conflict_ == action) outline(out, rect, 8.0f, ui::kWarn);
  else if (selected_ == action && frames_ / 15 % 2 == 0) outline(out, rect, 8.0f, ui::kInk);
  const ui::Box box = box_of(rect.x, rect.y, rect.w, rect.h);
  ui::rounded(out, atlas_, box, 8.0f, color);
  ui::shape(out, atlas_, panel == 2 ? ui::Shape::kRing : ui::Shape::kArrow,
            box_of(rect.x + 5.0f, rect.y + 5.0f, rect.w - 10.0f, rect.h - 10.0f), darker(color),
            kPanelTurn[static_cast<std::size_t>(panel)]);
  const std::string label = key_label(keys_.code(action));
  ui::text_centered(out, atlas_, box, label.size() > 3 ? 14.0f : 22.0f, label, letters);
}

void KeyMapScreen::draw_button(DrawList& out, int action) const {
  const Rect rect = target_rect(action);
  if (conflict_ == action) outline(out, rect, 3.0f, ui::kWarn);
  else if (selected_ == action && frames_ / 15 % 2 == 0) outline(out, rect, 3.0f, ui::kInk);
  ui::panel(out, atlas_, box_of(rect.x, rect.y, rect.w, rect.h), 3.0f, ui::kRaised, ui::kLine);
  ui::text_centered(out, atlas_, box_of(rect.x, rect.y + 6.0f, rect.w, 14.0f), 12.0f, action_name(action), ui::kSoft);
  ui::text_centered(out, atlas_, box_of(rect.x, rect.y + 22.0f, rect.w, 22.0f), 17.0f, key_label(keys_.code(action)), ui::kInk);
}

void KeyMapScreen::draw(DrawList& out) const {
  ++frames_;
  ui::fill(out, atlas_, {0.0f, 0.0f, 640.0f, 480.0f}, ui::kPanel);
  ui::text(out, atlas_, kMargin, 36.0f, 22.0f, text("Map controls", "Mapear controles"), ui::kInk);

  std::string status = text("Click a control, then press a key", "Haz clic en un control y presiona una tecla");
  Color status_color = ui::kSoft;
  if (conflict_ >= 0) {
    status = std::string(text("That key is used by: ", "Esa tecla ya la usa: ")) + action_name(conflict_);
    status_color = ui::kWarn;
  } else if (selected_ >= 0) {
    status = std::string(text("Press a key for: ", "Presiona una tecla para: ")) + action_name(selected_);
    status_color = ui::kInk;
  }
  ui::text_right(out, atlas_, 608.0f, 37.0f, 13.0f, status, status_color);
  ui::fill(out, atlas_, {kMargin, 60.0f, 576.0f, 1.0f}, ui::kLine);

  for (std::size_t pad = 0; pad < 2; ++pad) {
    const ui::Box box = {kPadX[pad] - 138.0f, 74.0f, 276.0f, 244.0f};
    ui::panel(out, atlas_, box, 6.0f, ui::kRaised, ui::kLine);
    ui::text_centered(out, atlas_, {box.x, 84.0f, box.w, 16.0f}, 13.0f, pad == 0 ? text("Player 1", "Jugador 1") : text("Player 2", "Jugador 2"), ui::kSoft);
  }
  for (int action = 0; action < 10; ++action) draw_panel(out, action);
  for (int action = kKeyCoin1; action < static_cast<int>(kKeyActions); ++action) draw_button(out, action);

  ui::button(out, atlas_, kResetBox, text("Reset", "Restablecer"), false, true);
  ui::button(out, atlas_, kSaveBox, text("Save", "Guardar"), true, true);
  ui::text_centered(out, atlas_, {182.0f, 412.0f, 276.0f, 36.0f}, 13.0f, text("Esc: cancel", "Esc: cancelar"), ui::kSoft);
}

}  // namespace piu
