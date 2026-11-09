// Plays a background animation (BGA): a set of sprite layers with keyframes.

#pragma once

#include <string>
#include <vector>

#include "core/assets.h"
#include "core/draw_list.h"
#include "core/song_data.h"

namespace piu {

struct Matrix {
  float a, b, c, d, e, f;
};

inline constexpr Matrix kIdentity = {1, 0, 0, 0, 1, 0};

// Textured quad in logical coordinates; u and v are in texture pixels.
void emit_quad(DrawList& out, const Texture& texture, float x, float y, float w, float h, float u0, float v0, float u1,
               float v1, const Matrix& m, Color color, int turn);

// One .bga resolved against the loaded assets.
class BgaPlayer {
 public:
  BgaPlayer() = default;
  // The Bga must outlive the player. Names it could not find go to missing.
  BgaPlayer(const Bga& bga, const Assets& assets, std::vector<std::string>& missing);

  // t is in frames of 1/60 s.
  void draw(DrawList& out, float t, float brightness) const;
  // One layer alone, moved by an offset.
  void draw_layer(DrawList& out, int slot, float t, float brightness, float dx = 0.0f, float dy = 0.0f) const;

 private:
  struct Quad {
    Texture texture;
    float x, y, w, h, u0, v0, u1, v1;
  };

  struct Layer {
    int slot = 0;
    const std::vector<BgaKey>* keys = nullptr;
    SpriteType type = SpriteType::kTile;
    int pattern_step = 0;
    int columns = 0;
    int rows = 0;
    std::vector<Quad> quads;
  };

  void draw_one(DrawList& out, const Layer& layer, float t, float brightness, float dx, float dy) const;

  std::vector<Layer> layers_;
};

}  // namespace piu
