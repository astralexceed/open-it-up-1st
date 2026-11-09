// Resolves BGA layers against the loaded assets and draws them at a given time.

#include "core/bga_player.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>

namespace piu {
namespace {

constexpr double kPi = 3.14159265358979323846;

float lerp(float a, float b, float f) { return a + (b - a) * f; }

}  // namespace

static void corner_uv(int turn, float px, float py, float u0, float v0, float u1, float v1, float& u, float& v) {
  const float du = u1 - u0;
  const float dv = v1 - v0;
  switch (turn) {
    case 1: u = u1 - du * py; v = v0 + dv * px; break;
    case 2: u = u1 - du * px; v = v1 - dv * py; break;
    case 3: u = u0 + du * py; v = v1 - dv * px; break;
    default: u = u0 + du * px; v = v0 + dv * py; break;
  }
}

void emit_quad(DrawList& out, const Texture& texture, float x, float y, float w, float h, float u0,
               float v0, float u1, float v1, const Matrix& m, Color color, int turn) {
  const auto tw = static_cast<float>(texture.width);
  const auto th = static_cast<float>(texture.height);
  inset_texels(u0, u1);
  inset_texels(v0, v1);
  const auto vertex = [&](float px, float py) {
    const float lx = x + w * px;
    const float ly = y + h * py;
    Vertex result{m.a * lx + m.b * ly + m.c, m.d * lx + m.e * ly + m.f, 0.0f, 0.0f, color};
    corner_uv(turn, px, py, u0 / tw, v0 / th, u1 / tw, v1 / th, result.u, result.v);
    return result;
  };
  out.quad(texture.id, vertex(0, 0), vertex(1, 0), vertex(1, 1), vertex(0, 1));
}

BgaPlayer::BgaPlayer(const Bga& bga, const Assets& assets, std::vector<std::string>& missing_out) {
  std::set<std::string> missing;
  for (const BgaLayer& layer : bga.layers) {
    if (layer.name.empty() || layer.keys.empty()) continue;
    Layer resolved;
    resolved.slot = layer.index;
    resolved.keys = &layer.keys;
    if (has_extension(layer.name, "spr")) {
      const Sprite* sprite = assets.sprite(layer.name);
      if (sprite == nullptr) {
        missing.insert(layer.name);
        continue;
      }
      resolved.type = sprite->type;
      resolved.pattern_step = sprite->pattern_step;
      resolved.columns = sprite->columns;
      resolved.rows = sprite->rows;
      for (const SpriteQuad& quad : sprite->quads) {
        const Texture* texture = assets.texture(quad.texture);
        if (texture == nullptr) missing.insert(quad.texture);
        resolved.quads.push_back({texture != nullptr ? *texture : Texture{}, quad.x, quad.y, quad.w,
                                  quad.h, quad.u0, quad.v0, quad.u1, quad.v1});
      }
    } else {
      const Texture* texture = assets.texture(layer.name);
      if (texture == nullptr) {
        missing.insert(layer.name);
        continue;
      }
      const auto w = static_cast<float>(texture->width);
      const auto h = static_cast<float>(texture->height);
      resolved.quads.push_back({*texture, 0, 0, w, h, 0, 0, w, h});
    }
    layers_.push_back(std::move(resolved));
  }
  missing_out.insert(missing_out.end(), missing.begin(), missing.end());
}

void BgaPlayer::draw(DrawList& out, float t, float brightness) const {
  for (const Layer& layer : layers_) draw_one(out, layer, t, brightness, 0.0f, 0.0f);
}

void BgaPlayer::draw_layer(DrawList& out, int slot, float t, float brightness, float dx, float dy) const {
  for (const Layer& layer : layers_) {
    if (layer.slot == slot) draw_one(out, layer, t, brightness, dx, dy);
  }
}

void BgaPlayer::draw_one(DrawList& out, const Layer& layer, float t, float brightness, float dx, float dy) const {
  {
    const std::optional<BgaPose> pose = bga_pose(*layer.keys, t);
    if (!pose || layer.quads.empty()) return;
    const BgaKey& a = *pose->from;
    const BgaKey& b = *pose->to;
    const float f = pose->fraction;

    const float x = lerp(a.x, b.x, f) + a.pivot_x + dx;
    const float y = lerp(a.y, b.y, f) + a.pivot_y + dy;
    const float scale = lerp(a.scale, b.scale, f);
    const auto angle = static_cast<double>(lerp(a.rotation, b.rotation, f)) * kPi / 180.0;
    const float c = static_cast<float>(std::cos(angle)) * scale;
    const float s = static_cast<float>(std::sin(angle)) * scale;
    const Matrix m = {c, s, x - c * a.pivot_x - s * a.pivot_y,
                      -s, c, y + s * a.pivot_x - c * a.pivot_y};
    const Color color = {lerp(a.red, b.red, f) * brightness, lerp(a.green, b.green, f) * brightness,
                         lerp(a.blue, b.blue, f) * brightness, lerp(a.alpha, b.alpha, f)};

    const auto draw = [&](const Quad& q, float qx, float qy) {
      if (q.texture.valid()) {
        emit_quad(out, q.texture, qx, qy, q.w, q.h, q.u0, q.v0, q.u1, q.v1, m, color, 0);
      }
    };

    const int n = static_cast<int>(layer.quads.size());
    if (layer.type == SpriteType::kAnimation) {
      const int frame = std::min(static_cast<int>(std::floor(static_cast<float>(n) * f + 0.5f)), n - 1);
      const Quad& q = layer.quads[static_cast<std::size_t>(frame)];
      draw(q, q.x, q.y);
    } else if (layer.type == SpriteType::kPattern) {
      const int start = static_cast<int>(std::floor(static_cast<float>(n) * (1.0f - f) + 0.5f));
      const bool use_step = layer.pattern_step != 0;
      const int outer = use_step ? layer.columns : layer.rows;
      const int inner = use_step ? layer.rows : layer.columns;
      int base = 0;
      for (int i = 0; i < outer; ++i) {
        for (int j = 0; j < inner; ++j) {
          const int index = (((start + base + j) % n) + n) % n;
          const Quad& q = layer.quads[static_cast<std::size_t>(index)];
          const int column = use_step ? i : j;
          const int row = use_step ? j : i;
          draw(q, q.x + q.w * static_cast<float>(column), q.y + q.h * static_cast<float>(row));
        }
        base += layer.pattern_step;
      }
    } else {
      for (const Quad& q : layer.quads) draw(q, q.x, q.y);
    }
  }
}

}  // namespace piu
