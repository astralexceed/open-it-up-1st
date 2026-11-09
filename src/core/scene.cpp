// Loads the flight models and camera path and builds the camera matrix.

#include "core/scene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

#include "core/json.h"

namespace piu {
namespace {

// First camera frame after each model range.
constexpr std::array<std::pair<int, int>, 18> kSceneRanges = {{{25, 0},   {51, 1},   {76, 2},   {101, 3},  {128, 4},
                                                               {153, 5},  {172, 6},  {198, 7},  {224, 8},  {263, 9},
                                                               {294, 10}, {319, 11}, {345, 12}, {403, 13}, {435, 14},
                                                               {467, 15}, {480, 16}, {499, 17}}};
constexpr float kNear = 1.0f;
constexpr float kFar = 16.0f;
constexpr float kPi = 3.14159265358979f;

using Vec3 = std::array<float, 3>;

Vec3 cross(const Vec3& p, const Vec3& q) {
  return {p[1] * q[2] - p[2] * q[1], p[2] * q[0] - p[0] * q[2], p[0] * q[1] - p[1] * q[0]};
}

float dot(const Vec3& p, const Vec3& q) { return p[0] * q[0] + p[1] * q[1] + p[2] * q[2]; }

Vec3 unit(const Vec3& p) {
  float length = std::sqrt(dot(p, p));
  if (length == 0.0f) length = 1.0f;
  return {p[0] / length, p[1] / length, p[2] / length};
}

std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  return text;
}

float number_at(const std::vector<Json>& items, std::size_t index) {
  return index < items.size() ? static_cast<float>(items[index].number()) : 0.0f;
}

}  // namespace

std::optional<Scene> parse_scene(std::string_view json) {
  const std::optional<Json> root = Json::parse(json);
  if (!root) return std::nullopt;
  const Json* models = root->find("models");
  const Json* materials = root->find("materials");
  const Json* camera = root->find("camera");
  if (models == nullptr || materials == nullptr || camera == nullptr) return std::nullopt;

  Scene scene;
  for (const Json& source : models->items()) {
    const Json* positions = source.find("positions");
    const Json* uvs = source.find("uvs");
    const Json* parts = source.find("parts");
    if (positions == nullptr || uvs == nullptr || parts == nullptr) return std::nullopt;

    SceneModel model;
    for (const Json& part_source : parts->items()) {
      const Json* material = part_source.find("material");
      const Json* faces = part_source.find("faces");
      if (material == nullptr || faces == nullptr) continue;
      const Json* file = materials->find(lower(material->string()));
      if (file == nullptr) continue;

      ScenePart part;
      part.texture = file->string();
      for (const Json& face : faces->items()) {
        const auto& f = face.items();
        if (f.size() < 6) continue;
        for (std::size_t i = 0; i < 3; ++i) {
          const auto position = static_cast<std::size_t>(f[i].number());
          const auto uv = static_cast<std::size_t>(f[i + 3].number());
          part.vertices.push_back(number_at(positions->items(), position * 3));
          part.vertices.push_back(number_at(positions->items(), position * 3 + 1));
          part.vertices.push_back(number_at(positions->items(), position * 3 + 2));
          part.vertices.push_back(number_at(uvs->items(), uv * 2));
          part.vertices.push_back(number_at(uvs->items(), uv * 2 + 1));
        }
      }
      model.parts.push_back(std::move(part));
    }
    scene.models.push_back(std::move(model));
  }

  for (const Json& source : camera->items()) {
    std::array<float, 6> key{};
    for (std::size_t i = 0; i < key.size(); ++i) key[i] = number_at(source.items(), i);
    scene.camera.push_back(key);
  }
  if (scene.models.empty() || scene.camera.empty()) return std::nullopt;
  return scene;
}

void request_scene_assets(const Scene& scene, Assets& assets) {
  for (const SceneModel& model : scene.models) {
    for (const ScenePart& part : model.parts) assets.request_texture(part.texture);
  }
}

void resolve_scene(Scene& scene, const Assets& assets) {
  for (SceneModel& model : scene.models) {
    for (ScenePart& part : model.parts) {
      const Texture* texture = assets.texture(part.texture);
      part.texture_id = texture != nullptr ? texture->id : 0;
    }
  }
}

int scene_base_model(int frame) {
  for (const auto& [end, model] : kSceneRanges) {
    if (frame < end) return model;
  }
  return 17;
}

std::array<float, 16> scene_matrix(const std::array<float, 6>& camera) {
  const Vec3 eye = {camera[0], camera[1], camera[2]};
  const Vec3 target = {camera[3], camera[4], camera[5]};
  const Vec3 f = unit({target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]});
  const Vec3 r = unit(cross(f, {0.0f, 0.0f, 1.0f}));
  const Vec3 u = cross(r, f);

  const float t = 1.0f / std::tan(40.0f * kPi / 180.0f);
  const float a = t / (4.0f / 3.0f);
  const float k = (kFar + kNear) / (kNear - kFar);
  const float l = 2.0f * kFar * kNear / (kNear - kFar);

  const std::array<float, 12> v = {r[0], r[1], r[2], -dot(r, eye), u[0], u[1], u[2], -dot(u, eye), -f[0], -f[1], -f[2], dot(f, eye)};
  return {a * v[0], t * v[4], k * v[8], -v[8], a * v[1], t * v[5], k * v[9], -v[9],
          a * v[2], t * v[6], k * v[10], -v[10], a * v[3], t * v[7], k * v[11] + l, -v[11]};
}

}  // namespace piu
