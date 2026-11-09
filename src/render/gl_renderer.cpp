// Shaders, sprite batches and the 3D flight background.

#include "render/gl_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace piu {
namespace {

const char* const kVertexShader = R"(
attribute vec2 a_position;
attribute vec2 a_uv;
attribute vec4 a_color;
varying vec2 v_uv;
varying vec4 v_color;
void main() {
  v_uv = a_uv;
  v_color = a_color;
  gl_Position = vec4(a_position.x / 320.0 - 1.0, 1.0 - a_position.y / 240.0, 0.0, 1.0);
}
)";

const char* const kFragmentShader = R"(
#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D u_texture;
varying vec2 v_uv;
varying vec4 v_color;
void main() {
  gl_FragColor = texture2D(u_texture, v_uv) * v_color;
}
)";

const char* const kSceneVertexShader = R"(
attribute vec3 a_position;
attribute vec2 a_uv;
uniform mat4 u_matrix;
varying vec2 v_uv;
varying float v_depth;
void main() {
  v_uv = a_uv;
  gl_Position = u_matrix * vec4(a_position, 1.0);
  v_depth = gl_Position.w;
}
)";

const char* const kSceneFragmentShader = R"(
#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D u_texture;
varying vec2 v_uv;
varying float v_depth;
void main() {
  vec4 color = texture2D(u_texture, v_uv);
  // Black fog: a straight line in depth that reaches black at 4/5 of the far plane.
  float fog = clamp(1.0 - v_depth / 12.8, 0.0, 1.0);
  // A choice of the port: 85% keeps the flight from competing with the arrows.
  gl_FragColor = vec4(color.rgb * 0.90 * fog, color.a);
}
)";

GLuint compile(GLenum type, const char* source) {
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (ok != GL_TRUE) {
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

const void* offset_of(std::size_t offset) { return reinterpret_cast<const void*>(offset); }

GLuint link(const char* vertex_source, const char* fragment_source) {
  const GLuint vertex = compile(GL_VERTEX_SHADER, vertex_source);
  const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragment_source);
  if (vertex == 0 || fragment == 0) return 0;
  const GLuint program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, fragment);
  glLinkProgram(program);
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  return linked == GL_TRUE ? program : 0;
}

}  // namespace

bool GlRenderer::init() {
  const GLuint vertex = compile(GL_VERTEX_SHADER, kVertexShader);
  const GLuint fragment = compile(GL_FRAGMENT_SHADER, kFragmentShader);
  if (vertex == 0 || fragment == 0) {
    return false;
  }

  program_ = glCreateProgram();
  glAttachShader(program_, vertex);
  glAttachShader(program_, fragment);
  glLinkProgram(program_);
  glDeleteShader(vertex);
  glDeleteShader(fragment);

  GLint linked = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    return false;
  }

  position_ = glGetAttribLocation(program_, "a_position");
  uv_ = glGetAttribLocation(program_, "a_uv");
  color_ = glGetAttribLocation(program_, "a_color");
  glGenBuffers(1, &buffer_);

  scene_program_ = link(kSceneVertexShader, kSceneFragmentShader);
  if (scene_program_ == 0) return false;
  scene_position_ = glGetAttribLocation(scene_program_, "a_position");
  scene_uv_ = glGetAttribLocation(scene_program_, "a_uv");
  scene_matrix_ = glGetUniformLocation(scene_program_, "u_matrix");

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  return true;
}

GLuint GlRenderer::begin_texture() {
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  return texture;
}

// The 640x480 picture is drawn into the largest box of the chosen shape.
void GlRenderer::view_size(int width, int height, int& view_width, int& view_height) const {
  const float aspect = wide_ ? 16.0f / 9.0f : static_cast<float>(kScreenWidth) / kScreenHeight;
  const float box_width = std::min(static_cast<float>(width), height * aspect);
  view_width = static_cast<int>(std::lround(box_width));
  view_height = static_cast<int>(std::lround(box_width / aspect));
}

void GlRenderer::to_screen(float px, float py, int width, int height, float& x, float& y) const {
  int view_width = 0;
  int view_height = 0;
  view_size(width, height, view_width, view_height);
  x = (px - static_cast<float>((width - view_width) / 2)) / static_cast<float>(view_width) * kScreenWidth;
  y = (py - static_cast<float>((height - view_height) / 2)) / static_cast<float>(view_height) * kScreenHeight;
}

void GlRenderer::render(const DrawList& list, int width, int height) {
  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  const auto& vertices = list.vertices();
  if (vertices.empty() && list.scene() == nullptr) {
    return;
  }

  int view_width = 0;
  int view_height = 0;
  view_size(width, height, view_width, view_height);
  glViewport((width - view_width) / 2, (height - view_height) / 2, view_width, view_height);

  if (list.scene() != nullptr) {
    draw_scene(list);
  }
  if (vertices.empty()) {
    return;
  }

  glUseProgram(program_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(), GL_STREAM_DRAW);
  glEnableVertexAttribArray(static_cast<GLuint>(position_));
  glEnableVertexAttribArray(static_cast<GLuint>(uv_));
  glEnableVertexAttribArray(static_cast<GLuint>(color_));
  const auto stride = static_cast<GLsizei>(sizeof(Vertex));
  glVertexAttribPointer(static_cast<GLuint>(position_), 2, GL_FLOAT, GL_FALSE, stride, offset_of(offsetof(Vertex, x)));
  glVertexAttribPointer(static_cast<GLuint>(uv_), 2, GL_FLOAT, GL_FALSE, stride, offset_of(offsetof(Vertex, u)));
  glVertexAttribPointer(static_cast<GLuint>(color_), 4, GL_FLOAT, GL_FALSE, stride, offset_of(offsetof(Vertex, color)));

  for (const Batch& batch : list.batches()) {
    glBindTexture(GL_TEXTURE_2D, batch.texture);
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(batch.first), static_cast<GLsizei>(batch.count));
  }
}

void GlRenderer::draw_scene(const DrawList& list) {
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  // Blending stays on: the flight is mixed by its texture alpha, with no alpha test.
  glUseProgram(scene_program_);
  glUniformMatrix4fv(scene_matrix_, 1, GL_FALSE, list.scene_matrix().data());
  glEnableVertexAttribArray(static_cast<GLuint>(scene_position_));
  glEnableVertexAttribArray(static_cast<GLuint>(scene_uv_));

  const Scene& scene = *list.scene();
  const int count = static_cast<int>(scene.models.size());
  for (int n = 0; n < 3; ++n) {
    const SceneModel& model = scene.models[static_cast<std::size_t>((list.scene_base_model() + n) % count)];
    for (const ScenePart& part : model.parts) {
      if (part.texture_id == 0 || part.vertices.empty()) continue;

      auto found = scene_buffers_.find(&part);
      if (found == scene_buffers_.end()) {
        GLuint buffer = 0;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(part.vertices.size() * sizeof(float)),
                     part.vertices.data(), GL_STATIC_DRAW);
        found = scene_buffers_.emplace(&part, buffer).first;
      }

      glBindBuffer(GL_ARRAY_BUFFER, found->second);
      const auto stride = static_cast<GLsizei>(5 * sizeof(float));
      glVertexAttribPointer(static_cast<GLuint>(scene_position_), 3, GL_FLOAT, GL_FALSE, stride, offset_of(0));
      glVertexAttribPointer(static_cast<GLuint>(scene_uv_), 2, GL_FLOAT, GL_FALSE, stride, offset_of(3 * sizeof(float)));
      glBindTexture(GL_TEXTURE_2D, part.texture_id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(part.vertices.size() / 5));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
  }

  glDisableVertexAttribArray(static_cast<GLuint>(scene_position_));
  glDisableVertexAttribArray(static_cast<GLuint>(scene_uv_));
  glDisable(GL_DEPTH_TEST);
}

}  // namespace piu
