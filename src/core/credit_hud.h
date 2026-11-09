// Credit counter and on-screen legends such as "press start" and "insert coin".

#pragma once

#include <vector>

#include "core/assets.h"
#include "core/coins.h"
#include "core/draw_list.h"

namespace piu {

// Credit counter and the legends around it.
class CreditHud {
 public:
  // The screen the original is in, which decides the legends.
  enum class Screen { kTitle, kSelect, kHelp, kPlay, kDemo };

  CreditHud() = default;
  explicit CreditHud(const Assets& assets);

  // p1 and p2 say who is playing. frame counts up once per 1/60 s.
  void draw(DrawList& out, const CoinBox& coins, Screen screen, bool p1, bool p2, bool double_play, int frame) const;
  // One sprite of gamescr.spr, with its corner at x, y.
  void draw_sprite_at(DrawList& out, int id, float x, float y) const { sprite_at(out, id, x, y); }

 private:
  struct Quad {
    Texture texture;
    float x, y, w, h, u0, v0, u1, v1;
  };

  void sprite(DrawList& out, int id, float dx, float dy, Color color) const;
  void sprite_at(DrawList& out, int id, float x, float y) const;
  void credit_text(DrawList& out, const CoinBox& coins) const;

  std::vector<Quad> quads_;
  Texture digits_;
};

}  // namespace piu
