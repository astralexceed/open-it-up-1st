// Draws the credit counter and its legends.

#include "core/credit_hud.h"

#include <algorithm>
#include <string>

#include "core/bga_player.h"

namespace piu {
namespace {

constexpr std::size_t kSpriteCount = 0x48;
constexpr float kPulseStep = 1.0f / 30.0f;
constexpr Color kWhite = {1.0f, 1.0f, 1.0f, 1.0f};

constexpr int kEventSprite = 6;
constexpr int kPleaseWaitP1 = 7;
constexpr int kPleaseWaitP2 = 8;
constexpr int kPressLeft = 0x22;
constexpr int kPressRight = 0x21;
constexpr int kInsertCoin = 0x27;
constexpr float kPressOffsetX = 0x28;
constexpr float kPressOffsetY = 0x5a;
constexpr float kInsertCoinOffset = 0xb4;

}  // namespace

CreditHud::CreditHud(const Assets& assets) {
  if (const Texture* texture = assets.texture("dec2")) digits_ = *texture;
  const Sprite* set = assets.sprite("gamescr.spr");
  if (set == nullptr) return;
  const std::size_t count = std::min(set->quads.size(), kSpriteCount);
  for (std::size_t i = 0; i < count; ++i) {
    const SpriteQuad& q = set->quads[i];
    const Texture* texture = assets.texture(q.texture);
    quads_.push_back({texture != nullptr ? *texture : Texture{}, q.x, q.y, q.w, q.h, q.u0, q.v0, q.u1, q.v1});
  }
}

void CreditHud::sprite(DrawList& out, int id, float dx, float dy, Color color) const {
  const auto index = static_cast<std::size_t>(id);
  if (index >= quads_.size() || !quads_[index].texture.valid()) return;
  const Quad& q = quads_[index];
  emit_quad(out, q.texture, q.x + dx, q.y + dy, q.w, q.h, q.u0, q.v0, q.u1, q.v1, kIdentity, color, 0);
}

void CreditHud::sprite_at(DrawList& out, int id, float x, float y) const {
  const auto index = static_cast<std::size_t>(id);
  if (index >= quads_.size() || !quads_[index].texture.valid()) return;
  const Quad& q = quads_[index];
  emit_quad(out, q.texture, x, y, q.w, q.h, q.u0, q.v0, q.u1, q.v1, kIdentity, kWhite, 0);
}

void CreditHud::credit_text(DrawList& out, const CoinBox& coins) const {
  // Label and digits are centered as one group.
  const std::string text = coins.text();
  const int total = static_cast<int>(text.size()) * 12 + 90;
  const int start = 320 - total / 2;
  sprite_at(out, 0x26, static_cast<float>(start), 428.0f);

  float x = static_cast<float>(start + 90);
  for (char c : text) {
    if (c == '/') {
      sprite_at(out, 0x24, x, 424.0f);
    } else if (c == '(') {
      sprite_at(out, 0x25, x, 423.0f);
    } else if (c == ')') {
      sprite_at(out, 0x23, x, 423.0f);
    } else if (digits_.valid()) {
      const int frame = static_cast<int>(c) + 0x10;
      const auto u0 = static_cast<float>(16 * (frame % 16));
      const auto v0 = static_cast<float>(32 * (frame / 16));
      emit_quad(out, digits_, x, 430.0f, 15.0f, 31.0f, u0, v0, u0 + 15.0f, v0 + 31.0f, kIdentity, kWhite, 0);
    }
    x += 12.0f;
  }
}

void CreditHud::draw(DrawList& out, const CoinBox& coins, Screen screen, bool p1, bool p2, bool double_play,
                     int frame) const {
  if (coins.free_play()) {
    if (screen != Screen::kPlay) sprite(out, kEventSprite, 0.0f, 0.0f, kWhite);
  } else {
    credit_text(out, coins);
  }

  const int phase = frame % 60;
  const float pulse = std::min(1.0f, phase < 16 ? 1.0f - static_cast<float>(phase + 10) * kPulseStep
                                                 : static_cast<float>(phase - 10) * kPulseStep);
  const Color pulsing = {1.0f, 1.0f, 1.0f, pulse};
  const bool has_credit = coins.credits() > 0 || coins.free_play();

  if (has_credit && (screen == Screen::kTitle || (screen == Screen::kSelect && !double_play))) {
    if (!p1) sprite(out, kPressLeft, -kPressOffsetX, kPressOffsetY, pulsing);
    if (!p2) sprite(out, kPressRight, kPressOffsetX, kPressOffsetY, pulsing);
  }
  if (has_credit && screen == Screen::kSelect && double_play && p1 != p2) {
    if (p1) sprite(out, kPleaseWaitP1, kPressOffsetX, kPressOffsetY, pulsing);
    else sprite(out, kPleaseWaitP2, -kPressOffsetX, kPressOffsetY, pulsing);
  }
  if (has_credit && screen == Screen::kPlay && !double_play && p1 != p2) {
    sprite(out, p1 ? kPleaseWaitP1 : kPleaseWaitP2, 0.0f, 0.0f, pulsing);
  }
  if (coins.coins() == 0 && !(p1 && p2) && (screen == Screen::kSelect || !double_play) && phase > 15) {
    const float offset = p1 ? kInsertCoinOffset : p2 ? -kInsertCoinOffset : 0.0f;
    sprite(out, kInsertCoin, offset, 0.0f, kWhite);
  }
}

}  // namespace piu
