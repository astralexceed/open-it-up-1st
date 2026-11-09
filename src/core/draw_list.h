// What a screen wants drawn this frame, independent of the graphics API.

#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace piu {

// Sprites are tiles packed edge to edge in one texture. Scaled with bilinear
// filtering, an edge would blend with the next tile and show as a line.
inline void inset_texels(float& first, float& last) {
  const float half = first <= last ? 0.5f : -0.5f;
  first += half;
  last -= half;
}

// Logical screen used by the original game. Renderers scale it to any size.
inline constexpr float kScreenWidth = 640.0f;
inline constexpr float kScreenHeight = 480.0f;

using TextureId = unsigned int;

struct Scene;

struct Color {
  float r, g, b, a;
};

struct Vertex {
  float x, y, u, v;
  Color color;
};

struct Batch {
  TextureId texture;
  std::size_t first;
  std::size_t count;
};

// Renderer-neutral textured geometry in logical coordinates, y pointing down.
class DrawList {
 public:
  void clear() {
    vertices_.clear();
    batches_.clear();
    scene_ = nullptr;
  }

  // A 3D background, drawn before all the 2D geometry.
  void set_scene(const Scene* scene, const std::array<float, 16>& matrix, int base_model) {
    scene_ = scene;
    scene_matrix_ = matrix;
    scene_base_model_ = base_model;
  }

  const Scene* scene() const { return scene_; }
  const std::array<float, 16>& scene_matrix() const { return scene_matrix_; }
  int scene_base_model() const { return scene_base_model_; }

  // Corners in order around the quad.
  void quad(TextureId texture, const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d) {
    if (batches_.empty() || batches_.back().texture != texture) {
      batches_.push_back({texture, vertices_.size(), 0});
    }
    vertices_.insert(vertices_.end(), {a, b, c, a, c, d});
    batches_.back().count += 6;
  }

  const std::vector<Vertex>& vertices() const { return vertices_; }
  const std::vector<Batch>& batches() const { return batches_; }

 private:
  std::vector<Vertex> vertices_;
  std::vector<Batch> batches_;
  const Scene* scene_ = nullptr;
  std::array<float, 16> scene_matrix_{};
  int scene_base_model_ = 0;
};

}  // namespace piu
