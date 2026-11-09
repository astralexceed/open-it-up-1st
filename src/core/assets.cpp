// Parses sprite sheets and keeps track of which assets are loaded, missing or pending.

#include "core/assets.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace piu {
namespace {

char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

std::string_view trim(std::string_view text) {
  const auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!text.empty() && blank(text.front())) text.remove_prefix(1);
  while (!text.empty() && blank(text.back())) text.remove_suffix(1);
  return text;
}

std::string upper_copy(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
  return out;
}

std::vector<std::string> tokens(std::string_view line) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
    const std::size_t start = i;
    while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r') ++i;
    if (i > start) out.emplace_back(line.substr(start, i - start));
  }
  return out;
}

float to_float(const std::string& text) { return static_cast<float>(std::strtod(text.c_str(), nullptr)); }
int to_int(const std::string& text) { return static_cast<int>(std::strtol(text.c_str(), nullptr, 10)); }

}  // namespace

std::string asset_key(std::string_view name) {
  name = trim(name);
  const std::size_t slash = name.find_last_of("\\/");
  if (slash != std::string_view::npos) name.remove_prefix(slash + 1);
  const std::size_t dot = name.find_last_of('.');
  if (dot != std::string_view::npos) name = name.substr(0, dot);
  std::string out(name);
  std::transform(out.begin(), out.end(), out.begin(), lower);
  return out;
}

bool has_extension(std::string_view name, std::string_view extension) {
  name = trim(name);
  if (name.size() <= extension.size() || name[name.size() - extension.size() - 1] != '.') return false;
  const std::string_view tail = name.substr(name.size() - extension.size());
  return std::equal(tail.begin(), tail.end(), extension.begin(),
                    [](char a, char b) { return lower(a) == lower(b); });
}

Sprite parse_sprite(std::string_view text) {
  Sprite sprite;
  while (!text.empty()) {
    const std::size_t end = text.find('\n');
    const std::string_view line = text.substr(0, end);
    text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);

    const std::vector<std::string> t = tokens(line);
    if (t.empty()) continue;
    const std::string head = upper_copy(t[0]);
    if (head == "TYPE") {
      const std::string kind = t.size() > 1 ? upper_copy(t[1]) : "";
      if (kind == "ANI") {
        sprite.type = SpriteType::kAnimation;
      } else if (kind == "PATTERN") {
        sprite.type = SpriteType::kPattern;
        sprite.pattern_step = t.size() > 2 ? to_int(t[2]) : 0;
        sprite.columns = t.size() > 3 ? to_int(t[3]) : 0;
        sprite.rows = t.size() > 4 ? to_int(t[4]) : 0;
      } else {
        sprite.type = SpriteType::kTile;
      }
    } else if (head == "T" && t.size() >= 10) {
      sprite.quads.push_back({t[1], to_float(t[2]), to_float(t[3]), to_float(t[4]), to_float(t[5]),
                              to_float(t[6]), to_float(t[7]), to_float(t[8]), to_float(t[9])});
    }
  }
  return sprite;
}

void Assets::request_texture(std::string_view name) {
  std::string key = asset_key(name);
  if (textures_.find(key) != textures_.end()) return;
  textures_.emplace(key, Texture{});
  requests_.push_back("tex:" + std::move(key));
}

void Assets::request_sprite(std::string_view name) {
  std::string key = asset_key(name);
  if (sprites_.find(key) != sprites_.end()) return;
  sprites_.emplace(key, std::nullopt);
  requests_.push_back("spr:" + std::move(key));
}

std::optional<std::string> Assets::next_request() {
  if (requests_.empty()) return std::nullopt;
  std::string next = std::move(requests_.front());
  requests_.pop_front();
  return next;
}

void Assets::set_texture(std::string_view key, Texture texture) {
  textures_.insert_or_assign(std::string(key), texture);
}

void Assets::set_sprite(std::string_view key, std::string_view text) {
  Sprite sprite = parse_sprite(text);
  for (const SpriteQuad& quad : sprite.quads) request_texture(quad.texture);
  sprites_.insert_or_assign(std::string(key), std::move(sprite));
}

void Assets::mark_texture_missing(std::string_view key) { set_texture(key, Texture{}); }

void Assets::mark_sprite_missing(std::string_view key) {
  sprites_.insert_or_assign(std::string(key), std::nullopt);
}

const Texture* Assets::texture(std::string_view name) const {
  const auto it = textures_.find(asset_key(name));
  return it != textures_.end() && it->second.valid() ? &it->second : nullptr;
}

const Sprite* Assets::sprite(std::string_view name) const {
  const auto it = sprites_.find(asset_key(name));
  return it != sprites_.end() && it->second.has_value() ? &*it->second : nullptr;
}

}  // namespace piu
