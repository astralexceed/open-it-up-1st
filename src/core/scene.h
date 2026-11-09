// The 3D flight used as the background of the remixes.

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/assets.h"

namespace piu {

struct ScenePart {
  std::string texture;
  // Interleaved x, y, z, u, v.
  std::vector<float> vertices;
  TextureId texture_id = 0;
};

struct SceneModel {
  std::vector<ScenePart> parts;
};

// The nonstop flight: track pieces plus one camera position per frame.
struct Scene {
  std::vector<SceneModel> models;
  // Eye x, y, z and target x, y, z.
  std::vector<std::array<float, 6>> camera;
};

std::optional<Scene> parse_scene(std::string_view json);
void request_scene_assets(const Scene& scene, Assets& assets);
void resolve_scene(Scene& scene, const Assets& assets);

// First of the three consecutive models visible at a camera frame.
int scene_base_model(int frame);
// Column-major view and projection matrix for a camera frame.
std::array<float, 16> scene_matrix(const std::array<float, 6>& camera);

}  // namespace piu
