// Loaded textures and sprite sheets, and the queue of files the engine still needs.

#pragma once

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/draw_list.h"

namespace piu {

struct Texture {
  TextureId id = 0;
  int width = 0;
  int height = 0;
  bool valid() const { return width > 0 && height > 0; }
};

struct SpriteQuad {
  std::string texture;
  float x, y, w, h, u0, v0, u1, v1;
};

enum class SpriteType { kTile, kAnimation, kPattern };

struct Sprite {
  SpriteType type = SpriteType::kTile;
  int pattern_step = 0;
  int columns = 0;
  int rows = 0;
  std::vector<SpriteQuad> quads;
};

Sprite parse_sprite(std::string_view text);

// Strips folders and the extension and lowers the case, as the game's files
// are referred to with inconsistent spelling.
std::string asset_key(std::string_view name);
bool has_extension(std::string_view name, std::string_view extension);

// Tracks what the game needs. The platform answers one request at a time, so
// the same flow works with asynchronous browser files and with a desktop disk.
class Assets {
 public:
  void request_texture(std::string_view name);
  void request_sprite(std::string_view name);

  // Next request as "tex:key" or "spr:key".
  std::optional<std::string> next_request();

  void set_texture(std::string_view key, Texture texture);
  void set_sprite(std::string_view key, std::string_view text);
  void mark_texture_missing(std::string_view key);
  void mark_sprite_missing(std::string_view key);

  const Texture* texture(std::string_view name) const;
  const Sprite* sprite(std::string_view name) const;

 private:
  std::deque<std::string> requests_;
  std::map<std::string, Texture, std::less<>> textures_;
  std::map<std::string, std::optional<Sprite>, std::less<>> sprites_;
};

}  // namespace piu
