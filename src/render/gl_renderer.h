// Draws a DrawList with OpenGL.

#pragma once

#include <unordered_map>

#include "core/draw_list.h"
#include "core/scene.h"
#include "render/gl.h"

namespace piu {

class GlRenderer {
 public:
  bool init();
  // Creates a texture and leaves it bound, ready for the platform to fill.
  GLuint begin_texture();
  // Scales the logical screen to fit, centered, in a framebuffer of any size.
  // Stretches the picture to 16:9 instead of the original 4:3.
  void set_wide(bool wide) { wide_ = wide; }
  void render(const DrawList& list, int width, int height);
  // Turns a position in a framebuffer of that size into logical screen coordinates.
  void to_screen(float px, float py, int width, int height, float& x, float& y) const;

 private:
  void view_size(int width, int height, int& view_width, int& view_height) const;

  bool wide_ = false;
  GLuint program_ = 0;
  GLuint buffer_ = 0;
  GLint position_ = -1;
  GLint uv_ = -1;
  GLint color_ = -1;

  void draw_scene(const DrawList& list);

  GLuint scene_program_ = 0;
  GLint scene_position_ = -1;
  GLint scene_uv_ = -1;
  GLint scene_matrix_ = -1;
  std::unordered_map<const ScenePart*, GLuint> scene_buffers_;
};

}  // namespace piu
