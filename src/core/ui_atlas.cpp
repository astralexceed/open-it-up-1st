// Draws the setup screens: the text atlas, rounded boxes and one line labels.

#include "core/ui_atlas.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>

#include "core/bga_player.h"
#include "core/ui_font_data.h"

namespace piu {
namespace {

constexpr int kShapeSize = 64;
constexpr int kArrowX = 0;
constexpr int kRingX = kShapeSize;
constexpr int kWhiteX = kShapeSize * 2;
constexpr int kCircleX = kShapeSize * 3;
constexpr int kSamples = 4;
// From this size up, the sharper large face is used when it has every letter.
constexpr float kDisplayFromSize = 20.0f;
// Inter's middle of the capital letters, as a fraction of the size.
constexpr float kCapMiddle = 0.364f;

struct Face {
  const UiGlyph* first;
  const UiGlyph* last;
  float em;
};

constexpr Face kBody = {std::begin(kUiBodyGlyphs), std::end(kUiBodyGlyphs), kUiBodyEm};
constexpr Face kDisplay = {std::begin(kUiDisplayGlyphs), std::end(kUiDisplayGlyphs), kUiDisplayEm};

const UiGlyph* find_glyph(const Face& face, std::uint32_t code) {
  for (const UiGlyph* glyph = face.first; glyph != face.last; ++glyph) {
    if (glyph->code == code) return glyph;
  }
  return nullptr;
}

// Calls visit with each code point of a UTF-8 line. Only one and two byte letters are used here.
template <typename Visit>
void each_code(std::string_view text, Visit visit) {
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto first = static_cast<unsigned char>(text[i]);
    if (first >= 0xC0 && first < 0xE0 && i + 1 < text.size()) {
      visit(static_cast<std::uint32_t>((first & 0x1F) << 6 | (static_cast<unsigned char>(text[i + 1]) & 0x3F)));
      ++i;
    } else {
      visit(static_cast<std::uint32_t>(first));
    }
  }
}

const Face& face_for(std::string_view text, float size) {
  if (size < kDisplayFromSize) return kBody;
  bool complete = true;
  each_code(text, [&](std::uint32_t code) { complete = complete && find_glyph(kDisplay, code) != nullptr; });
  return complete ? kDisplay : kBody;
}

float region_v() { return static_cast<float>(kUiFontHeight); }

void draw_region(DrawList& out, const Texture& atlas, const ui::Box& box, float u, float v, float w, float h, Color color,
                 int turn = 0) {
  emit_quad(out, atlas, box.x, box.y, box.w, box.h, u, v, u + w, v + h, kIdentity, color, turn);
}

void corner(DrawList& out, const Texture& atlas, float x, float y, float radius, int column, int row, Color color) {
  const float half = kShapeSize / 2.0f;
  draw_region(out, atlas, {x, y, radius, radius}, static_cast<float>(kCircleX) + half * static_cast<float>(column),
              region_v() + half * static_cast<float>(row), half, half, color);
}

}  // namespace

UiAtlas make_ui_atlas() {
  UiAtlas atlas{kUiFontWidth, kUiFontHeight + kShapeSize,
                std::vector<std::uint8_t>(static_cast<std::size_t>(kUiFontWidth) * (kUiFontHeight + kShapeSize) * 4, 0)};
  const auto set = [&](int x, int y, int coverage) {
    std::uint8_t* pixel = &atlas.rgba[(static_cast<std::size_t>(y) * kUiFontWidth + static_cast<std::size_t>(x)) * 4];
    pixel[0] = pixel[1] = pixel[2] = 255;
    pixel[3] = static_cast<std::uint8_t>(coverage);
  };

  int x = 0;
  int y = 0;
  for (const std::uint8_t run : kUiFontRuns) {
    const int coverage = (run & 0x0F) * 17;
    for (int i = 0; i <= run >> 4; ++i) {
      if (coverage != 0) set(x, y, coverage);
      if (++x == kUiFontWidth) {
        x = 0;
        ++y;
      }
    }
  }

  const auto draw_shape = [&](int left, const std::function<bool(float, float)>& inside) {
    for (int py = 0; py < kShapeSize; ++py) {
      for (int px = 0; px < kShapeSize; ++px) {
        int hits = 0;
        for (int sy = 0; sy < kSamples; ++sy) {
          for (int sx = 0; sx < kSamples; ++sx) {
            hits += inside(static_cast<float>(px) + (static_cast<float>(sx) + 0.5f) / kSamples,
                           static_cast<float>(py) + (static_cast<float>(sy) + 0.5f) / kSamples)
                        ? 1
                        : 0;
          }
        }
        if (hits != 0) set(left + px, kUiFontHeight + py, hits * 255 / (kSamples * kSamples));
      }
    }
  };
  // An arrow pointing up-left: a triangular head and a diagonal shaft.
  draw_shape(kArrowX, [](float px, float py) {
    const bool head = px >= 8.0f && py >= 8.0f && px + py < 52.0f;
    const bool shaft = std::fabs(px - py) < 9.0f && px + py >= 40.0f && px + py < 106.0f;
    return head || shaft;
  });
  draw_shape(kRingX, [](float px, float py) {
    const float distance = std::hypot(px - 32.0f, py - 32.0f);
    return distance > 14.0f && distance < 25.0f;
  });
  draw_shape(kWhiteX, [](float, float) { return true; });
  draw_shape(kCircleX, [](float px, float py) { return std::hypot(px - 32.0f, py - 32.0f) <= 32.0f; });
  return atlas;
}

namespace ui {

void fill(DrawList& out, const Texture& atlas, const Box& box, Color color) {
  draw_region(out, atlas, box, kWhiteX + 8.0f, region_v() + 8.0f, 8.0f, 8.0f, color);
}

void rounded(DrawList& out, const Texture& atlas, const Box& box, float radius, Color color) {
  const float r = std::min({radius, box.w / 2.0f, box.h / 2.0f});
  const float inner_w = box.w - r * 2.0f;
  const float inner_h = box.h - r * 2.0f;
  corner(out, atlas, box.x, box.y, r, 0, 0, color);
  corner(out, atlas, box.x + box.w - r, box.y, r, 1, 0, color);
  corner(out, atlas, box.x, box.y + box.h - r, r, 0, 1, color);
  corner(out, atlas, box.x + box.w - r, box.y + box.h - r, r, 1, 1, color);
  fill(out, atlas, {box.x + r, box.y, inner_w, r}, color);
  fill(out, atlas, {box.x + r, box.y + box.h - r, inner_w, r}, color);
  fill(out, atlas, {box.x, box.y + r, box.w, inner_h}, color);
}

void panel(DrawList& out, const Texture& atlas, const Box& box, float radius, Color color, Color border) {
  rounded(out, atlas, box, radius, border);
  rounded(out, atlas, {box.x + 1.0f, box.y + 1.0f, box.w - 2.0f, box.h - 2.0f}, std::max(0.0f, radius - 1.0f), color);
}

void shape(DrawList& out, const Texture& atlas, Shape which, const Box& box, Color color, int turn) {
  const auto u = static_cast<float>(which == Shape::kRing ? kRingX : kArrowX);
  draw_region(out, atlas, box, u, region_v(), kShapeSize, kShapeSize, color, turn);
}

float text_width(std::string_view value, float size) {
  const Face& face = face_for(value, size);
  float advance = 0.0f;
  each_code(value, [&](std::uint32_t code) {
    if (const UiGlyph* glyph = find_glyph(face, code)) advance += static_cast<float>(glyph->advance) / 256.0f;
  });
  return advance * size / face.em;
}

void text(DrawList& out, const Texture& atlas, float x, float center_y, float size, std::string_view value, Color color) {
  const Face& face = face_for(value, size);
  const float scale = size / face.em;
  const float baseline = center_y + size * kCapMiddle;
  each_code(value, [&](std::uint32_t code) {
    const UiGlyph* glyph = find_glyph(face, code);
    if (glyph == nullptr) return;
    if (glyph->w > 0 && glyph->h > 0) {
      draw_region(out, atlas,
                  {x + static_cast<float>(glyph->left) * scale, baseline + static_cast<float>(glyph->top) * scale,
                   static_cast<float>(glyph->w) * scale, static_cast<float>(glyph->h) * scale},
                  static_cast<float>(glyph->x), static_cast<float>(glyph->y), glyph->w, glyph->h, color);
    }
    x += static_cast<float>(glyph->advance) / 256.0f * scale;
  });
}

void text_centered(DrawList& out, const Texture& atlas, const Box& box, float size, std::string_view value, Color color) {
  text(out, atlas, box.x + (box.w - text_width(value, size)) / 2.0f, box.y + box.h / 2.0f, size, value, color);
}

void text_right(DrawList& out, const Texture& atlas, float right, float center_y, float size, std::string_view value,
                Color color) {
  text(out, atlas, right - text_width(value, size), center_y, size, value, color);
}

void button(DrawList& out, const Texture& atlas, const Box& box, std::string_view label, bool primary, bool enabled) {
  if (primary && enabled) {
    rounded(out, atlas, box, 3.0f, kBlue);
    text_centered(out, atlas, box, 14.0f, label, kOnLight);
    return;
  }
  panel(out, atlas, box, 3.0f, enabled ? kRaised : kPanel, kLine);
  text_centered(out, atlas, box, 14.0f, label, enabled ? kInk : kLine);
}

}  // namespace ui
}  // namespace piu
