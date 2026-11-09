// The screen where the player reassigns the keys, drawn with the two dance pads.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/assets.h"
#include "core/draw_list.h"
#include "core/key_map.h"

namespace piu {

// The letters and shapes of this screen, as RGBA pixels. It needs no game data.
struct KeyMapAtlas {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;
};
KeyMapAtlas make_key_map_atlas();

class KeyMapScreen {
 public:
  enum class Result { kRunning, kSaved, kCancelled };

  KeyMapScreen(const KeyMap& keys, bool spanish, const Texture& atlas);

  void press_key(const std::string& code);
  void click(float x, float y);
  void draw(DrawList& out) const;

  Result result() const { return result_; }
  const KeyMap& keys() const { return keys_; }
  int selected() const { return selected_; }

 private:
  struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  };

  static Rect target_rect(int action);
  const char* text(const char* en, const char* es) const { return spanish_ ? es : en; }
  std::string action_name(int action) const;
  void fill(DrawList& out, const Rect& rect, Color color) const;
  void outline(DrawList& out, const Rect& rect, Color color) const;
  void write(DrawList& out, float x, float y, float scale, const std::string& value, Color color) const;
  void write_centered(DrawList& out, const Rect& rect, float scale, const std::string& value, Color color) const;
  void draw_panel(DrawList& out, int action) const;
  void draw_button(DrawList& out, int action) const;

  KeyMap keys_;
  bool spanish_;
  Texture atlas_;
  int selected_ = -1;
  int conflict_ = -1;
  Result result_ = Result::kRunning;
  mutable int frames_ = 0;
};

}  // namespace piu
