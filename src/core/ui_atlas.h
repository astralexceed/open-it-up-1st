// The look shared by the setup screens: Inter text, rounded panels and the launcher's colors.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/assets.h"
#include "core/draw_list.h"

namespace piu {

// The letters and shapes of the setup screens, as RGBA pixels. It needs no game data.
struct UiAtlas {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;
};
UiAtlas make_ui_atlas();

namespace ui {

inline constexpr Color kPanel = {0.090f, 0.082f, 0.157f, 1.0f};
inline constexpr Color kRaised = {0.133f, 0.122f, 0.227f, 1.0f};
inline constexpr Color kLine = {0.208f, 0.184f, 0.349f, 1.0f};
inline constexpr Color kInk = {0.925f, 0.914f, 0.969f, 1.0f};
inline constexpr Color kSoft = {0.655f, 0.631f, 0.769f, 1.0f};
inline constexpr Color kYellow = {1.0f, 0.812f, 0.200f, 1.0f};
inline constexpr Color kSky = {0.529f, 0.808f, 0.922f, 1.0f};
inline constexpr Color kBlue = {0.286f, 0.706f, 1.0f, 1.0f};
inline constexpr Color kRed = {0.898f, 0.282f, 0.302f, 1.0f};
inline constexpr Color kWarn = {1.0f, 0.541f, 0.361f, 1.0f};
inline constexpr Color kOnLight = {0.031f, 0.071f, 0.110f, 1.0f};

enum class Shape { kArrow, kRing };

struct Box {
  float x, y, w, h;
  bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

void fill(DrawList& out, const Texture& atlas, const Box& box, Color color);
void rounded(DrawList& out, const Texture& atlas, const Box& box, float radius, Color color);
// A rounded box with a one pixel border.
void panel(DrawList& out, const Texture& atlas, const Box& box, float radius, Color color, Color border);
void shape(DrawList& out, const Texture& atlas, Shape which, const Box& box, Color color, int turn);

float text_width(std::string_view text, float size);
// Draws one line of UTF-8 text; center_y is the middle of the line.
void text(DrawList& out, const Texture& atlas, float x, float center_y, float size, std::string_view value, Color color);
void text_centered(DrawList& out, const Texture& atlas, const Box& box, float size, std::string_view value, Color color);
void text_right(DrawList& out, const Texture& atlas, float right, float center_y, float size, std::string_view value,
                Color color);

// A button in the launcher's style. Primary is the blue one.
void button(DrawList& out, const Texture& atlas, const Box& box, std::string_view label, bool primary, bool enabled);

}  // namespace ui
}  // namespace piu
